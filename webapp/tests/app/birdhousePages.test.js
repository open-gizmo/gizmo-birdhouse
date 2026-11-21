import { beforeEach, describe, expect, it, vi } from 'vitest';
import { readBody } from 'h3';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import VisitsPage from '~/pages/visits.vue';
import DetectionPage from '~/pages/detection.vue';
import DevicePage from '~/pages/device.vue';
import { loadVisits, removeVisit, VISITS_PAGE_SIZE } from '~/composables/useBirdhouse.js';
import { buildVisitPhotoUrl } from '~/composables/useDeviceApi.js';
import { buildVisitFileName, formatVisitTime } from '~/composables/useUtils.js';
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

// Visits on the card, newest first, as GET /api/visits answers them (the 2nd has no time: the clock wasn't set)
const makeVisits = count => Array.from({ length: count }, (_, index) => ({
    id: count - index,
    timestamp: index === 1 ? null : 1763721600 - index * 3600,
    sizeBytes: 150000
}));

// The device: its status, its visits, its settings, and what the app sent
let status;
let visits;
let settings;
let sentSettings;
let deletedIds;
let visitQueries;

// Answer like the device
const registerDeviceEndpoints = () => {
    registerEndpoint('/api/camera', () => ({ errorId: null, errorMessage: null, data: cameraDetails }));
    registerEndpoint('/api/device', () => ({ errorId: null, errorMessage: null, data: deviceDetails }));
    registerEndpoint('/api/status', () => ({ errorId: null, errorMessage: null, data: status }));
    registerEndpoint('/api/visits', event => {
        const query = Object.fromEntries(new URL(event.node.req.url, 'http://device').searchParams);
        visitQueries.push(query);
        const offset = Number(query.offset);
        const limit = Number(query.limit);
        return { errorId: null, errorMessage: null, data: { total: visits.length, offset, visits: visits.slice(offset, offset + limit) } };
    });
    registerEndpoint('/api/visits/delete', {
        method: 'POST',
        handler: async event => {
            const body = await readBody(event);
            deletedIds.push(body.id);
            visits = visits.filter(visit => visit.id !== body.id);
            return { errorId: null, errorMessage: null, data: null };
        }
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

// Start each test from an app that has read nothing yet, and a birdhouse with three visits
beforeEach(() => {
    status = {
        detection: { enabled: true, active: true, motionPercent: 1 },
        storage: { available: true, totalBytes: 32 * 1024 ** 3, usedBytes: 1.5 * 1024 ** 3 },
        visitCount: 3,
        lastVisit: { id: 3, timestamp: 1763721600, sizeBytes: 150000 },
        clockSet: true
    };
    visits = makeVisits(3);
    settings = { detectionEnabled: true, sensitivity: 'medium', cooldownSeconds: 30, photoResolution: 'SXGA', flash: false };
    sentSettings = null;
    deletedIds = [];
    visitQueries = [];
    shownDialogs.length = 0;
    confirmDialogs.length = 0;
    const globalStore = useGlobalStore();
    Object.assign(globalStore.value, {
        cameraLoaded: false,
        cameraLoadFailed: false,
        status: null,
        visits: [],
        visitTotal: 0,
        visitsLoaded: false,
        visitsLoadFailed: false,
        settingsLoaded: false,
        settingsLoadFailed: false
    });
    vi.spyOn(console, 'error').mockImplementation(() => {});
    registerDeviceEndpoints();
});

// Helpers of the visits: the photo address, the file name and the time
describe('visit helpers', () => {
    // The small picture and the photo come from the same route
    it('builds the photo addresses', () => {
        expect(buildVisitPhotoUrl({ id: 12 })).toBe('/api/visits/photo?id=12');
        expect(buildVisitPhotoUrl({ id: 12, isThumbnail: true })).toBe('/api/visits/photo?id=12&thumbnail=1');
    });

    // A saved photo is named by its number and its time, or its number alone
    it('names the saved photos', () => {
        expect(buildVisitFileName({ id: 4, timestamp: null })).toBe('visit-4.jpg');
        expect(buildVisitFileName({ id: 4, timestamp: 1763721600 })).toMatch(/^visit-4-\d{4}-\d{2}-\d{2}-\d{2}-\d{2}\.jpg$/);
    });

    // A visit without a time has no date to show
    it('formats the time of a visit', () => {
        expect(formatVisitTime({ timestamp: null, locale: 'en' })).toBe('');
        expect(formatVisitTime({ timestamp: 1763721600, locale: 'en' })).toContain('2025');
    });
});

// Reading the visits: pages of the firmware, newest first
describe('loadVisits', () => {
    // The first page comes with the status, and the next one goes on from it
    it('reads the visits page by page', async () => {
        visits = makeVisits(VISITS_PAGE_SIZE + 2);
        await loadVisits();
        const globalStore = useGlobalStore();
        expect(globalStore.value.visits).toHaveLength(VISITS_PAGE_SIZE);
        expect(globalStore.value.visitTotal).toBe(VISITS_PAGE_SIZE + 2);
        expect(globalStore.value.status.detection.active).toBe(true);
        await loadVisits({ more: true });
        expect(globalStore.value.visits).toHaveLength(VISITS_PAGE_SIZE + 2);
        expect(visitQueries.map(query => query.offset)).toEqual(['0', String(VISITS_PAGE_SIZE)]);
    });

    // A bird that comes by while reading moves the pages by one: the visit at the edge isn't shown twice
    it('never shows a visit twice', async () => {
        visits = makeVisits(VISITS_PAGE_SIZE + 2);
        await loadVisits();
        visits = makeVisits(VISITS_PAGE_SIZE + 3);
        await loadVisits({ more: true });
        const ids = useGlobalStore().value.visits.map(visit => visit.id);
        expect(new Set(ids).size).toBe(ids.length);
    });

    // A removed visit leaves the list, and the status follows
    it('removes a visit', async () => {
        await loadVisits();
        await removeVisit(3);
        const globalStore = useGlobalStore();
        expect(deletedIds).toEqual([3]);
        expect(globalStore.value.visits.map(visit => visit.id)).toEqual([2, 1]);
        expect(globalStore.value.status.visitCount).toBe(2);
        expect(globalStore.value.status.lastVisit.id).toBe(2);
    });
});

// Visits page: the status of the birdhouse and the visits
describe('visits page', () => {
    // The status, and a small picture for each visit
    it('shows the status and the visits', async () => {
        const wrapper = await mountSuspended(VisitsPage);
        await vi.waitFor(() => expect(wrapper.findAll('li img')).toHaveLength(3));
        expect(wrapper.text()).toContain('Watching');
        expect(wrapper.text()).toContain('1.5 GB of 32.0 GB used');
        expect(wrapper.text()).toContain('3 visits on the card.');
        expect(wrapper.text()).toContain('Visit 2, time unknown');
        expect(wrapper.find('li img').attributes('src')).toBe('/api/visits/photo?id=3&thumbnail=1');
        expect(wrapper.text()).toContain('Delete all visits');
    });

    // Without a card, the page says why there are no visits
    it('explains a missing card', async () => {
        status.storage = { available: false, totalBytes: 0, usedBytes: 0 };
        visits = [];
        const wrapper = await mountSuspended(VisitsPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('No microSD card'));
        expect(wrapper.text()).toContain('No card');
        expect(wrapper.text()).not.toContain('Delete all visits');
    });

    // A birdhouse with no visits yet says what will happen
    it('explains an empty birdhouse', async () => {
        visits = [];
        status.visitCount = 0;
        status.lastVisit = null;
        const wrapper = await mountSuspended(VisitsPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('No visits yet'));
        expect(wrapper.text()).toContain('None yet');
    });

    // A switched-off detection reads as off, and a clock that isn't set is explained
    it('shows a switched-off detection and an unset clock', async () => {
        status.detection = { enabled: false, active: false, motionPercent: 0 };
        status.clockSet = false;
        const wrapper = await mountSuspended(VisitsPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Off'));
        expect(wrapper.text()).toContain('new visits have no date');
    });

    // Only older visits load with the button
    it('offers the older visits', async () => {
        visits = makeVisits(VISITS_PAGE_SIZE + 1);
        const wrapper = await mountSuspended(VisitsPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Show older visits'));
    });
});

// Detection page: the settings of the visit detection
describe('detection page', () => {
    // The form starts from the settings of the device
    it('shows the settings of the device', async () => {
        settings.sensitivity = 'high';
        settings.cooldownSeconds = 120;
        const wrapper = await mountSuspended(DetectionPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Sensitivity'));
        expect(wrapper.text()).toContain('High');
        expect(wrapper.text()).toContain('2 min');
        expect(wrapper.text()).toContain('SXGA');
        expect(wrapper.text()).toContain('Sensor OV2640');
    });

    // Saving sends the settings, after a confirmation
    it('saves the settings', async () => {
        const wrapper = await mountSuspended(DetectionPage);
        await vi.waitFor(() => expect(wrapper.find('form').exists()).toBe(true));
        useGlobalStore().value.settings.flash = true;
        await wrapper.find('form').trigger('submit');
        expect(confirmDialogs).toHaveLength(1);
        await confirmDialogs[0].onConfirm();
        expect(sentSettings).toMatchObject({ detectionEnabled: true, sensitivity: 'medium', cooldownSeconds: 30, photoResolution: 'SXGA', flash: true });
        expect(shownDialogs[0].message).toBe('Settings saved. The birdhouse uses them right away.');
    });

    // Settings that couldn't be read give no form, so the defaults of the store are never saved over the settings of the device
    it('offers no form without the settings of the device', async () => {
        registerEndpoint('/api/settings', { method: 'GET', handler: () => { throw new Error('offline'); } });
        const wrapper = await mountSuspended(DetectionPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain("Couldn't read the settings"));
        expect(wrapper.find('form').exists()).toBe(false);
    });
});

// Device page: the device card of the layer, with the reset of the birdhouse
describe('device page', () => {
    // The reset joins the destructive rows of the card
    it('offers to reset the saved settings', async () => {
        const wrapper = await mountSuspended(DevicePage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Forget network'));
        expect(wrapper.text()).toContain('Reset saved settings');
    });
});
