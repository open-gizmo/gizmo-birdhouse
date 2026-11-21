import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Stub the Nitro globals the routes of this app use before the handler modules load (the mock of the gizmo-common layer imports its h3 functions, so it runs on real h3 events)
vi.hoisted(() => {
    globalThis.defineEventHandler = handler => handler;
    globalThis.defineNitroPlugin = plugin => plugin;
    globalThis.useAppConfig = () => ({ gizmo: { name: 'Gizmo Birdhouse', hostname: 'gizmo-birdhouse' } });
});

// Load the handlers of the mock device API
const { default: getCamera } = await import('../../server/api/camera/index.get.js');
const { default: getCapture } = await import('../../server/api/capture/index.get.js');
const { default: getStatus } = await import('../../server/api/status/index.get.js');
const { default: getVisits } = await import('../../server/api/visits/index.get.js');
const { default: getVisitPhoto } = await import('../../server/api/visits/photo.get.js');
const { default: deleteVisit } = await import('../../server/api/visits/delete.post.js');
const { default: deleteAllVisits } = await import('../../server/api/visits/delete-all.post.js');
const { default: getSettings } = await import('../../server/api/settings/index.get.js');
const { default: postSettings } = await import('../../server/api/settings/index.post.js');
const { default: resetSettings } = await import('../../server/api/reset-settings/index.post.js');
const { getEspMockState } = await import('../../server/utils/esp-mock-state.js');
const { default: nameMockDevice } = await import('../../server/plugins/gizmo-mock.js');
const { buildDevicePayload, resetGizmoMockState } = await import('gizmo-common/web/server/utils/gizmo-mock.js');

// Build a real h3 request event for a path with its query, and a JSON body for a POST
const buildEvent = (url = '/', body) => {
    const request = new IncomingMessage(new Socket());
    request.method = body === undefined ? 'GET' : 'POST';
    request.url = url;
    if (body !== undefined) {
        const text = typeof body === 'string' ? body : JSON.stringify(body);
        request.headers['content-type'] = 'application/json';
        request.headers['content-length'] = String(Buffer.byteLength(text)); // h3 reads no body without its length
        request.push(text);
        request.push(null);
    }
    return createEvent(request, new ServerResponse(request));
};

// Call a handler and return its answer with the status code and headers it set
const callHandler = async (handler, event = buildEvent()) => {
    const response = await handler(event);
    return { statusCode: event.node.res.statusCode, headers: event.node.res.getHeaders(), response };
};

// Start every test from the demo device the dev server boots with
beforeEach(() => {
    resetGizmoMockState();
});

// GET /api/camera: what the camera can do, like the firmware
describe('mock camera', () => {
    // The OV2640 of the ESP32-CAM, up to UXGA, with the flash
    it('describes the camera of the ESP32-CAM', async () => {
        const { statusCode, response } = await callHandler(getCamera);
        expect(statusCode).toBe(200);
        expect(response.errorId).toBeNull();
        expect(response.data.available).toBe(true);
        expect(response.data.sensor).toBe('OV2640');
        expect(response.data.defaultResolution).toBe('UXGA');
        expect(response.data.resolutions.at(-1)).toEqual({ name: 'UXGA', width: 1600, height: 1200 });
        expect(response.data.quality).toEqual({ min: 4, max: 63, default: 10 });
        expect(response.data.hasFlash).toBe(true);
    });

    // The device card of the layer shows the name and the address of the birdhouse
    it('names the mock device like the birdhouse', async () => {
        nameMockDevice();
        const payload = buildDevicePayload({ state: getEspMockState() });
        expect(payload.name).toBe('Gizmo Birdhouse');
        expect(payload.hostname).toBe('gizmo-birdhouse.local');
    });
});

// GET /api/capture: the same options, errors and headers as the firmware
describe('mock capture', () => {
    // Fake timers, so the time of a snapshot doesn't slow the tests down
    beforeEach(() => {
        vi.useFakeTimers();
    });

    // Call the capture handler, letting its snapshot time pass
    const capture = async url => {
        const pending = callHandler(getCapture, buildEvent(url));
        await vi.runAllTimersAsync();
        vi.useRealTimers();
        return pending;
    };

    // Without options: the largest resolution, with the headers of the firmware
    it('takes a snapshot at the largest resolution by default', async () => {
        const { statusCode, headers, response } = await capture('/api/capture');
        expect(statusCode).toBe(200);
        expect(headers['x-resolution']).toBe('1600x1200');
        expect(headers['x-capture-time-ms']).toBe('300');
        expect(headers['cache-control']).toBe('no-store');
        expect(response).toContain('width="1600"');
    });

    // The options are read whatever the case of the resolution, and the flash takes longer
    it('reads the options of the request', async () => {
        const { headers, response } = await capture('/api/capture?resolution=vga&quality=20&flash=true');
        expect(headers['x-resolution']).toBe('640x480');
        expect(headers['x-capture-time-ms']).toBe('450');
        expect(response).toContain('Q20');
    });

    // Every refused option answers the error of the firmware
    it.each([
        ['resolution=4K', 'invalid_resolution'],
        ['resolution=FHD', 'resolution_not_supported'],
        ['quality=3', 'invalid_quality'],
        ['quality=10x', 'invalid_quality'],
        ['flash=yes', 'invalid_flash']
    ])('refuses %s with %s', async (query, errorId) => {
        const { statusCode, response } = await capture(`/api/capture?${query}`);
        expect(statusCode).toBe(400);
        expect(response).toMatchObject({ errorId, data: null });
    });

    // A sensor that didn't start answers 503, like the firmware
    it('refuses snapshots without a sensor', async () => {
        getEspMockState().camera.isAvailable = false;
        const { statusCode, response } = await capture('/api/capture');
        expect(statusCode).toBe(503);
        expect(response.errorId).toBe('camera_unavailable');
    });
});
// GET /api/status: the detection, the card and the visits, like the firmware
describe('mock status', () => {
    // The demo birdhouse watches, with a card and a few visits
    it('describes the demo birdhouse', async () => {
        const { statusCode, response } = await callHandler(getStatus);
        expect(statusCode).toBe(200);
        expect(response.data.detection).toMatchObject({ enabled: true, active: true });
        expect(response.data.storage.available).toBe(true);
        expect(response.data.storage.usedBytes).toBeLessThan(response.data.storage.totalBytes);
        expect(response.data.visitCount).toBe(getEspMockState().birdhouse.visits.length);
        expect(response.data.lastVisit.id).toBe(getEspMockState().birdhouse.visits.at(-1).id);
        expect(response.data.clockSet).toBe(true);
    });

    // Without a card, the detection pauses and no visit is listed
    it('pauses without a card', async () => {
        getEspMockState().birdhouse.storage.available = false;
        const { response } = await callHandler(getStatus);
        expect(response.data.detection).toMatchObject({ enabled: true, active: false });
        expect(response.data.visitCount).toBe(0);
        expect(response.data.lastVisit).toBeNull();
    });
});

// GET /api/visits and /api/visits/photo: the pages and the photos, like the firmware
describe('mock visits', () => {
    // The newest visit comes first, and a page stops at its limit
    it('lists the visits newest first', async () => {
        const { response } = await callHandler(getVisits, buildEvent('/api/visits?offset=1&limit=2'));
        const visits = getEspMockState().birdhouse.visits;
        expect(response.data.total).toBe(visits.length);
        expect(response.data.offset).toBe(1);
        expect(response.data.visits.map(visit => visit.id)).toEqual([visits.at(-2).id, visits.at(-3).id]);
    });

    // A visit taken before the clock was set has no time
    it('sends a missing time as null', async () => {
        getEspMockState().birdhouse.visits.at(-1).timestamp = 0;
        const { response } = await callHandler(getVisits, buildEvent('/api/visits'));
        expect(response.data.visits[0].timestamp).toBeNull();
    });

    // Every refused page answers the error of the firmware
    it.each(['offset=-1', 'offset=1.5', 'limit=0', 'limit=51', 'limit=abc'])('refuses %s', async query => {
        const { statusCode, response } = await callHandler(getVisits, buildEvent(`/api/visits?${query}`));
        expect(statusCode).toBe(400);
        expect(response.errorId).toBe('invalid_page');
    });

    // The photo and its small picture are kept by the browser: a number is never given twice
    it('sends the photo of a visit', async () => {
        const { statusCode, headers, response } = await callHandler(getVisitPhoto, buildEvent('/api/visits/photo?id=1&thumbnail=1'));
        expect(statusCode).toBe(200);
        expect(headers['cache-control']).toBe('private, max-age=31536000, immutable');
        expect(response).toContain('width="320"');
    });

    // A missing or unknown visit answers the error of the firmware
    it.each([
        ['', 400, 'invalid_visit_id'],
        ['id=0', 400, 'invalid_visit_id'],
        ['id=abc', 400, 'invalid_visit_id'],
        ['id=999', 404, 'visit_not_found']
    ])('refuses the photo of "%s"', async (query, status, errorId) => {
        const { statusCode, response } = await callHandler(getVisitPhoto, buildEvent(`/api/visits/photo?${query}`));
        expect(statusCode).toBe(status);
        expect(response.errorId).toBe(errorId);
    });

    // A removed visit leaves the list, and an unknown one is refused
    it('removes a visit', async () => {
        const { response } = await callHandler(deleteVisit, buildEvent('/api/visits/delete', { id: 1 }));
        expect(response.errorId).toBeNull();
        expect(getEspMockState().birdhouse.visits.some(visit => visit.id === 1)).toBe(false);
        const again = await callHandler(deleteVisit, buildEvent('/api/visits/delete', { id: 1 }));
        expect(again.statusCode).toBe(404);
        const invalid = await callHandler(deleteVisit, buildEvent('/api/visits/delete', { id: '1' }));
        expect(invalid.response.errorId).toBe('invalid_visit_id');
    });

    // Every visit goes at once, unless an earlier removal is still running or there is no card
    it('removes every visit', async () => {
        const state = getEspMockState();
        state.birdhouse.isDeleteAllPending = true;
        expect((await callHandler(deleteAllVisits, buildEvent('/api/visits/delete-all', {}))).response.errorId).toBe('visits_busy');
        state.birdhouse.isDeleteAllPending = false;
        expect((await callHandler(deleteAllVisits, buildEvent('/api/visits/delete-all', {}))).response.errorId).toBeNull();
        expect(state.birdhouse.visits).toEqual([]);
        state.birdhouse.storage.available = false;
        expect((await callHandler(deleteAllVisits, buildEvent('/api/visits/delete-all', {}))).statusCode).toBe(503);
    });
});

// GET and POST /api/settings, and POST /api/reset-settings: the rules of the firmware
describe('mock settings', () => {
    // A new birdhouse has the defaults of the firmware
    it('starts from the defaults of the firmware', async () => {
        const { response } = await callHandler(getSettings);
        expect(response.data).toEqual({ detectionEnabled: true, sensitivity: 'medium', cooldownSeconds: 30, photoResolution: 'SXGA', flash: false });
    });

    // The fields present are saved, with the firmware's own case for the resolution, and the saved settings come back
    it('saves the fields present', async () => {
        const { response } = await callHandler(postSettings, buildEvent('/api/settings', { sensitivity: 'high', photoResolution: 'vga' }));
        expect(response.data).toMatchObject({ sensitivity: 'high', photoResolution: 'VGA', cooldownSeconds: 30 });
    });

    // Every refused field is named, like the firmware, and changes nothing
    it.each([
        [{ detectionEnabled: 1 }, 'detectionEnabled'],
        [{ sensitivity: 'extreme' }, 'sensitivity'],
        [{ cooldownSeconds: 4 }, 'cooldownSeconds'],
        [{ cooldownSeconds: 3601 }, 'cooldownSeconds'],
        [{ cooldownSeconds: 30.5 }, 'cooldownSeconds'],
        [{ photoResolution: 'FHD' }, 'photoResolution'],
        [{ flash: 'yes' }, 'flash'],
        [{ sensitivity: 'low', flash: 'yes' }, 'flash']
    ])('refuses %o', async (body, field) => {
        const { statusCode, response } = await callHandler(postSettings, buildEvent('/api/settings', body));
        expect(statusCode).toBe(400);
        expect(response).toMatchObject({ errorId: 'invalid_settings_field', errorMessage: field });
        expect(getEspMockState().birdhouse.settings.sensitivity).toBe('medium');
    });

    // The reset clears the settings and the network, and keeps the visits
    it('resets the settings and the network', async () => {
        const state = getEspMockState();
        state.birdhouse.settings.sensitivity = 'high';
        const visitCount = state.birdhouse.visits.length;
        const { response } = await callHandler(resetSettings, buildEvent('/api/reset-settings', {}));
        expect(response.errorId).toBeNull();
        expect(state.birdhouse.settings.sensitivity).toBe('medium');
        expect(state.savedSsid).toBe('');
        expect(state.birdhouse.visits).toHaveLength(visitCount);
    });
});
