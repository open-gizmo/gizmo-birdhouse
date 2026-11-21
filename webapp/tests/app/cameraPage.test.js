import { beforeEach, describe, expect, it, vi } from 'vitest';
import { getQuery, readBody } from 'h3';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import CameraPage from '~/pages/camera.vue';
import DevicePage from '~/pages/device.vue';
import { captureSnapshot, loadCamera } from '~/composables/useCamera.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Remember the dialogs instead of showing them (the tests check what they say, and confirm them)
const shownDialogs = vi.hoisted(() => []);
const confirmDialogs = vi.hoisted(() => []);
vi.mock('@brumaombra/ui-vintage/message-dialog', () => ({
    showMessageDialog: dialog => shownDialogs.push(dialog)
}));
vi.mock('@brumaombra/ui-vintage/confirm-dialog', () => ({
    showConfirmDialog: dialog => confirmDialogs.push(dialog)
}));

// The camera of the ESP32-CAM, as GET /api/camera answers it
const cameraDetails = {
    available: true,
    sensor: 'OV2640',
    resolutions: [
        { name: 'QQVGA', width: 160, height: 120 },
        { name: 'VGA', width: 640, height: 480 },
        { name: 'SXGA', width: 1280, height: 1024 },
        { name: 'UXGA', width: 1600, height: 1200 }
    ],
    defaultResolution: 'UXGA',
    quality: { min: 4, max: 63, default: 10 },
    hasFlash: true
};

// Details of the device, as GET /api/device answers them (the device card of the gizmo-common layer reads them)
const deviceDetails = {
    name: 'Gizmo Birdhouse',
    version: 'v1.0.0',
    uptimeSeconds: 93784,
    freeHeap: 187432,
    ssid: 'Home',
    accessPointActive: false,
    ip: '192.168.1.42',
    hostname: 'gizmo-birdhouse.local',
    signalQuality: 74
};

// The device API: its camera, the next snapshot error, the queries of the snapshots, its settings and the settings the app sent
let camera;
let snapshotErrorId;
let snapshotQueries;
let settings;
let sentSettings;

// Answer like the device
const registerDeviceEndpoints = () => {
    registerEndpoint('/api/camera', () => ({ errorId: null, errorMessage: null, data: camera }));
    registerEndpoint('/api/device', () => ({ errorId: null, errorMessage: null, data: deviceDetails }));
    registerEndpoint('/api/capture', event => {
        snapshotQueries.push(getQuery(event));

        // Fail like the firmware: its own JSON error body (not the h3 error wrapper)
        if (snapshotErrorId) {
            event.node.res.statusCode = 503;
            return { errorId: snapshotErrorId, errorMessage: 'Busy', data: null };
        }

        // Send a small image with the headers of the firmware
        event.node.res.setHeader('X-Resolution', '1600x1200');
        event.node.res.setHeader('X-Capture-Time-Ms', '412');
        event.node.res.setHeader('Content-Type', 'image/jpeg');
        return Buffer.alloc(2048, 1);
    });
    registerEndpoint('/api/settings', {
        method: 'GET',
        handler: () => ({ errorId: null, errorMessage: null, data: settings })
    });
    registerEndpoint('/api/settings', {
        method: 'POST',
        handler: async event => {
            sentSettings = await readBody(event);
            settings = { ...settings, ...sentSettings };
            return { errorId: null, errorMessage: null, data: settings };
        }
    });
};

// Start each test from a page that has read neither the camera nor the settings yet
beforeEach(() => {
    camera = structuredClone(cameraDetails);
    snapshotErrorId = null;
    snapshotQueries = [];
    settings = { detectionEnabled: true, sensitivity: 'medium', cooldownSeconds: 30, photoResolution: 'SXGA', flash: false };
    sentSettings = null;
    shownDialogs.length = 0;
    confirmDialogs.length = 0;
    resetGizmoStore();
    const globalStore = useGlobalStore();
    globalStore.value.cameraLoaded = false;
    globalStore.value.cameraLoadFailed = false;
    globalStore.value.snapshotOptions = { resolution: '', quality: 10, flash: false };
    globalStore.value.snapshot = null;
    globalStore.value.settingsLoaded = false;
    globalStore.value.settingsLoadFailed = false;
    vi.spyOn(console, 'error').mockImplementation(() => {});
    vi.spyOn(URL, 'createObjectURL').mockReturnValue('blob:snapshot'); // Node's URL refuses the Blob of the simulated browser
    vi.spyOn(URL, 'revokeObjectURL').mockImplementation(() => {});
    registerDeviceEndpoints();
});

// Reading the camera: the options start from what the sensor can do
describe('loadCamera', () => {
    // The largest resolution of the sensor and the default quality are chosen
    it('starts from the defaults of the camera', async () => {
        await loadCamera();
        const globalStore = useGlobalStore();
        expect(globalStore.value.cameraLoaded).toBe(true);
        expect(globalStore.value.snapshotOptions).toEqual({ resolution: 'UXGA', quality: 10, flash: false });
    });

    // A resolution already chosen stays when the camera can still take it
    it('keeps a resolution the camera can take', async () => {
        const globalStore = useGlobalStore();
        globalStore.value.snapshotOptions.resolution = 'VGA';
        await loadCamera();
        expect(globalStore.value.snapshotOptions.resolution).toBe('VGA');
    });
});

// Taking a snapshot: the options go to the device, and the image and its details come back
describe('captureSnapshot', () => {
    // The options are sent as the query of the firmware, and the headers fill the details
    it('sends the options and keeps the snapshot', async () => {
        await loadCamera();
        const globalStore = useGlobalStore();
        globalStore.value.snapshotOptions = { resolution: 'VGA', quality: 20, flash: true };
        await captureSnapshot();
        expect(snapshotQueries).toEqual([{ resolution: 'VGA', quality: '20', flash: '1' }]);
        expect(globalStore.value.snapshot).toMatchObject({ resolution: '1600x1200', sizeBytes: 2048, captureTimeMs: 412 });
        expect(globalStore.value.snapshot.url).toBe('blob:snapshot');
    });

    // The error of the device arrives as a blob (the request asks for an image): it must be read, or the dialog can't translate it
    it('shows the translated error of the device', async () => {
        snapshotErrorId = 'camera_busy';
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Take snapshot'));
        await wrapper.findAll('button').find(button => button.text().includes('Take snapshot')).trigger('click');
        await vi.waitFor(() => expect(shownDialogs.length).toBe(1));
        expect(shownDialogs[0].message).toBe('The camera is busy with another photo (yours, or a visit). Try again in a moment.');
    });
});

// Camera page: the live look inside, then the settings of the visit detection
describe('camera page', () => {
    // A working camera shows the snapshot options, with the flash, and the settings below them
    it('shows the options of the camera and the settings', async () => {
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.find('form').exists()).toBe(true));
        expect(wrapper.text()).toContain('No snapshot yet');
        expect(wrapper.text()).toContain('UXGA');
        expect(wrapper.text()).toContain('High');
        expect(wrapper.text()).toContain('Flash');
        expect(wrapper.text()).toContain('Visit detection');
        expect(wrapper.text()).toContain('SXGA');
    });

    // A sensor that didn't start gets an explanation instead of a snapshot button that can only fail, and no settings
    it('explains a camera that did not start', async () => {
        camera.available = false;
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Camera not found'));
        expect(wrapper.text()).not.toContain('Take snapshot');
        expect(wrapper.find('form').exists()).toBe(false);
    });

    // The form starts from the settings of the device, with the long waits in hours
    it('shows the settings of the device', async () => {
        settings.sensitivity = 'high';
        settings.cooldownSeconds = 3600;
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.find('form').exists()).toBe(true));
        expect(wrapper.find('form').text()).toContain('High');
        expect(wrapper.find('form').text()).toContain('1 h');
    });

    // Saving sends the settings, after a confirmation
    it('saves the settings', async () => {
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.find('form').exists()).toBe(true));
        useGlobalStore().value.settings.flash = true;
        await wrapper.find('form').trigger('submit');
        expect(confirmDialogs).toHaveLength(1);
        await confirmDialogs[0].onConfirm();
        expect(sentSettings).toMatchObject({ detectionEnabled: true, sensitivity: 'medium', cooldownSeconds: 30, photoResolution: 'SXGA', flash: true });
        expect(shownDialogs[0].message).toBe('Settings saved. The birdhouse uses them right away.');
    });

    // Settings that couldn't be read give no form, so the defaults of the store are never saved over the settings of the device (the snapshot still works)
    it('offers no form without the settings of the device', async () => {
        registerEndpoint('/api/settings', { method: 'GET', handler: () => { throw new Error('offline'); } });
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain("Couldn't read the settings"));
        expect(wrapper.find('form').exists()).toBe(false);
        expect(wrapper.text()).toContain('Take snapshot');
    });
});

// Device page: the device card of the layer, like in every OpenGizmo app
describe('device page', () => {
    // The card of the layer reads GET /api/device by itself
    it('shows the device card', async () => {
        const wrapper = await mountSuspended(DevicePage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Restart'));
        expect(wrapper.text()).toContain('Forget network');
    });

    // The camera page is only about the camera
    it('keeps the device card off the camera page', async () => {
        const wrapper = await mountSuspended(CameraPage);
        await vi.waitFor(() => expect(wrapper.find('form').exists()).toBe(true));
        expect(wrapper.text()).not.toContain('Forget network');
    });
});
