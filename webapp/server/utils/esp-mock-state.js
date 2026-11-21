// The Wi-Fi and device mock (its state, the responses and errors of the firmware) comes from the gizmo-common layer: this file adds the camera, the visits and the settings of the birdhouse
import { EspMockError, getGizmoMockState } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Resolutions of the firmware, smallest to largest (capture_options.h)
export const RESOLUTIONS = [
    { name: 'QQVGA', width: 160, height: 120 },
    { name: 'QVGA', width: 320, height: 240 },
    { name: 'CIF', width: 400, height: 296 },
    { name: 'VGA', width: 640, height: 480 },
    { name: 'SVGA', width: 800, height: 600 },
    { name: 'XGA', width: 1024, height: 768 },
    { name: 'HD', width: 1280, height: 720 },
    { name: 'SXGA', width: 1280, height: 1024 },
    { name: 'UXGA', width: 1600, height: 1200 },
    { name: 'FHD', width: 1920, height: 1080 },
    { name: 'QXGA', width: 2048, height: 1536 },
    { name: 'QHD', width: 2560, height: 1440 },
    { name: 'WQXGA', width: 2560, height: 1600 },
    { name: 'QSXGA', width: 2560, height: 1920 }
];

// JPEG quality range of the firmware (lower numbers mean better quality)
export const JPEG_QUALITY = Object.freeze({
    min: 4,
    max: 63,
    default: 10
});

// Settings of the firmware (birdhouse_settings.h): the sensitivities, the cooldown range and the defaults of a new birdhouse
export const SENSITIVITIES = ['low', 'medium', 'high'];
export const COOLDOWN_SECONDS = Object.freeze({
    min: 5,
    max: 3600,
    default: 30
});
export const defaultSettings = () => ({
    detectionEnabled: true,
    sensitivity: 'medium',
    cooldownSeconds: COOLDOWN_SECONDS.default,
    photoResolution: 'SXGA',
    flash: false
});

// Most visits on a page of GET /api/visits (visits.cpp)
export const PAGE_SIZE = Object.freeze({
    default: 20,
    max: 50
});

// Size of the demo microSD card: 32 GB, with some space taken by other files
const CARD_TOTAL_BYTES = 31914983424;
const CARD_OTHER_BYTES = 52428800;

// Visits of the demo birdhouse: a few birds over the last two days, newest last (like the firmware's list)
const demoVisits = (now = Date.now()) => {
    const minutesAgo = [2900, 2350, 1500, 1460, 700, 640, 95, 12];
    return minutesAgo.map((minutes, index) => ({
        id: index + 1,
        timestamp: Math.floor((now - minutes * 60000) / 1000),
        sizeBytes: 140000 + index * 7919
    }));
};

// Birdhouse of the demo device: watching, with a card and a few visits
const demoBirdhouse = () => {
    const visits = demoVisits();
    return {
        settings: defaultSettings(),
        visits,
        lastVisitId: visits.at(-1).id,
        storage: { available: true, totalBytes: CARD_TOTAL_BYTES },
        motionPercent: 1,
        clockSet: true,
        isDeleteAllPending: false // The visits deleted all at once are still being removed (for tests)
    };
};

// Camera of the demo device: the OV2640 of the ESP32-CAM, up to UXGA
const demoCamera = () => ({
    isAvailable: true,
    sensorName: 'OV2640',
    maxResolutionIndex: RESOLUTIONS.findIndex(resolution => resolution.name === 'UXGA')
});

// Get the mock device state: the one of the layer (Wi-Fi, uptime, kept across hot reloads) plus the camera and the birdhouse
export const getEspMockState = () => {
    const state = getGizmoMockState();
    state.camera ??= demoCamera();
    state.birdhouse ??= demoBirdhouse();
    return state;
};

// Build what the camera can do, like GET /api/camera of the firmware
export const buildCameraPayload = camera => ({
    available: camera.isAvailable,
    sensor: camera.sensorName,
    resolutions: RESOLUTIONS.slice(0, camera.maxResolutionIndex + 1),
    defaultResolution: RESOLUTIONS[camera.maxResolutionIndex].name,
    quality: { ...JPEG_QUALITY },
    hasFlash: true
});

// Read the options of a snapshot request with the same rules and errors as the firmware (each value undefined when the request leaves it out)
export const parseCaptureOptions = ({ query, camera }) => {
    const options = { resolutionIndex: camera.maxResolutionIndex, quality: JPEG_QUALITY.default, useFlash: false };

    // Read the resolution, which the sensor must be able to take
    if (query.resolution !== undefined) {
        const resolutionIndex = RESOLUTIONS.findIndex(resolution => resolution.name.toLowerCase() === String(query.resolution).toLowerCase());
        if (resolutionIndex === -1) {
            throw new EspMockError(400, 'invalid_resolution', 'Unknown resolution');
        }
        if (resolutionIndex > camera.maxResolutionIndex) {
            throw new EspMockError(400, 'resolution_not_supported', 'Resolution not supported by the sensor');
        }
        options.resolutionIndex = resolutionIndex;
    }

    // Read the JPEG quality: a whole number of one or two digits, in range
    if (query.quality !== undefined) {
        const quality = String(query.quality);
        if (!/^\d{1,2}$/.test(quality) || Number(quality) < JPEG_QUALITY.min || Number(quality) > JPEG_QUALITY.max) {
            throw new EspMockError(400, 'invalid_quality', 'Quality must be a whole number from 4 to 63');
        }
        options.quality = Number(quality);
    }

    // Read the flash switch: 1 or true turns it on, 0 or false leaves it off
    if (query.flash !== undefined) {
        const flash = String(query.flash).toLowerCase();
        if (!['0', '1', 'false', 'true'].includes(flash)) {
            throw new EspMockError(400, 'invalid_flash', 'Flash must be 0 or 1');
        }
        options.useFlash = flash === '1' || flash === 'true';
    }

    // Return the options once they are all valid
    return options;
};

// Draw the test card the mock sends instead of a photo (an SVG at the size of the snapshot, brighter with the flash)
export const buildTestCard = ({ resolution, quality, useFlash, takenAt = new Date() }) => {
    const { width, height } = resolution;
    const fontSize = Math.max(10, Math.round(height / 12));
    const background = useFlash ? '#fff4d6' : '#1b222d';
    const foreground = useFlash ? '#1b222d' : '#f7931a';
    const time = takenAt.toLocaleTimeString('en-GB');

    // Return the SVG of the test card
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`
        + `<rect width="${width}" height="${height}" fill="${background}"/>`
        + `<circle cx="${width / 2}" cy="${height / 2}" r="${Math.min(width, height) / 3}" fill="none" stroke="${foreground}" stroke-width="${Math.max(2, fontSize / 6)}"/>`
        + `<text x="50%" y="46%" text-anchor="middle" font-family="sans-serif" font-size="${fontSize}" fill="${foreground}">Gizmo Birdhouse</text>`
        + `<text x="50%" y="58%" text-anchor="middle" font-family="sans-serif" font-size="${fontSize / 2.6}" fill="${foreground}">${resolution.name} ${width}x${height} · Q${quality} · ${time}</text>`
        + '</svg>';
};
/******************************** Birdhouse ********************************/

// Write a visit like the firmware: its number, its time (null when the clock wasn't set) and the size of its photo
export const buildVisitPayload = visit => ({
    id: visit.id,
    timestamp: visit.timestamp || null,
    sizeBytes: visit.sizeBytes
});

// Bytes the visits take on the card
const getUsedBytes = birdhouse => CARD_OTHER_BYTES + birdhouse.visits.reduce((total, visit) => total + visit.sizeBytes, 0);

// Build what the birdhouse is doing, like GET /api/status of the firmware
export const buildStatusPayload = state => {
    const { birdhouse, camera } = state;
    const isActive = birdhouse.settings.detectionEnabled && camera.isAvailable && birdhouse.storage.available;
    const visits = birdhouse.storage.available ? birdhouse.visits : [];
    return {
        detection: {
            enabled: birdhouse.settings.detectionEnabled,
            active: isActive,
            motionPercent: isActive ? birdhouse.motionPercent : 0
        },
        storage: {
            available: birdhouse.storage.available,
            totalBytes: birdhouse.storage.available ? birdhouse.storage.totalBytes : 0,
            usedBytes: birdhouse.storage.available ? getUsedBytes(birdhouse) : 0
        },
        visitCount: visits.length,
        lastVisit: visits.length > 0 ? buildVisitPayload(visits.at(-1)) : null,
        clockSet: birdhouse.clockSet
    };
};

// Read a whole number of a query like the firmware: digits only, up to 4294967295 (undefined otherwise)
export const parseWholeNumber = value => {
    const text = String(value);
    if (!/^\d{1,10}$/.test(text) || Number(text) > 4294967295) return undefined;
    return Number(text);
};

// Read the page of a visit list request with the rules and errors of the firmware
export const parseVisitPage = query => {
    const page = { offset: 0, limit: PAGE_SIZE.default };
    if (query.offset !== undefined) {
        page.offset = parseWholeNumber(query.offset);
        if (page.offset === undefined) throw new EspMockError(400, 'invalid_page', 'offset must be a whole number');
    }
    if (query.limit !== undefined) {
        page.limit = parseWholeNumber(query.limit);
        if (!page.limit || page.limit > PAGE_SIZE.max) throw new EspMockError(400, 'invalid_page', 'limit must be a whole number from 1 to 50');
    }
    return page;
};

// Build a page of visits, newest first, like GET /api/visits of the firmware
export const buildVisitsPayload = ({ state, offset, limit }) => {
    const { birdhouse } = state;
    const visits = birdhouse.storage.available ? [...birdhouse.visits].reverse() : [];
    return {
        total: visits.length,
        offset,
        visits: visits.slice(offset, offset + limit).map(buildVisitPayload)
    };
};

// Find a visit of the card, or fail like the firmware
export const findVisit = ({ state, id }) => {
    const { birdhouse } = state;
    if (!birdhouse.storage.available) {
        throw new EspMockError(503, 'storage_unavailable', 'No microSD card');
    }
    const visit = birdhouse.visits.find(candidate => candidate.id === id);
    if (!visit) {
        throw new EspMockError(404, 'visit_not_found', 'Visit not found');
    }
    return visit;
};

// Check the settings of a request like the firmware, in its order (missing or null fields keep their value): the name of the first invalid field, or null
export const findInvalidSettingsField = ({ body, camera }) => {
    const maxResolutionIndex = camera.maxResolutionIndex;
    const checks = {
        detectionEnabled: value => typeof value === 'boolean',
        sensitivity: value => SENSITIVITIES.includes(value),
        cooldownSeconds: value => Number.isInteger(value) && value >= COOLDOWN_SECONDS.min && value <= COOLDOWN_SECONDS.max,
        photoResolution: value => {
            if (typeof value !== 'string') return false;
            const index = RESOLUTIONS.findIndex(resolution => resolution.name.toLowerCase() === value.toLowerCase());
            return index !== -1 && index <= maxResolutionIndex;
        },
        flash: value => typeof value === 'boolean'
    };
    return Object.keys(checks).find(field => body[field] != null && !checks[field](body[field])) || null;
};

// Apply the settings of a request once they are all valid (the resolution is written with the firmware's own case)
export const applySettings = ({ settings, body }) => {
    for (const field of Object.keys(settings)) {
        if (body[field] == null) continue;
        settings[field] = field === 'photoResolution' ? RESOLUTIONS.find(resolution => resolution.name.toLowerCase() === body[field].toLowerCase()).name : body[field];
    }
};

// Draw the photo of a visit: a bird on its nest, seen from the camera inside the box (an SVG stands in for the JPEG of the card)
export const buildVisitPhoto = ({ visit, isThumbnail }) => {
    const width = isThumbnail ? 320 : 1280;
    const height = isThumbnail ? 240 : 1024;
    const hue = (visit.id * 47) % 360;
    const offset = (visit.id % 3) * 24;
    const label = visit.timestamp ? new Date(visit.timestamp * 1000).toISOString().slice(0, 16).replace('T', ' ') : 'time unknown';

    // Return the SVG of the photo: the wooden walls, the light of the entrance hole, the nest and the bird on it
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 320 240">`
        + '<defs><radialGradient id="light" cx="0.78" cy="0.18" r="0.75"><stop offset="0" stop-color="#8a6a48"/><stop offset="1" stop-color="#2a2019"/></radialGradient></defs>'
        + '<rect width="320" height="240" fill="url(#light)"/>'
        + '<path d="M64 0 V240 M128 0 V240 M192 0 V240 M256 0 V240" stroke="#1f1813" stroke-opacity="0.5" stroke-width="2"/>'
        + '<circle cx="250" cy="44" r="22" fill="#f3e6c4"/>'
        + '<ellipse cx="160" cy="214" rx="150" ry="40" fill="#5a4128"/>'
        + `<g transform="translate(${100 + offset} 112)">`
        + `<path d="M8 58 L-26 50 L4 74 Z" fill="hsl(${hue} 40% 38%)"/>`
        + `<ellipse cx="50" cy="62" rx="48" ry="36" fill="hsl(${hue} 50% 52%)"/>`
        + `<circle cx="84" cy="26" r="24" fill="hsl(${hue} 50% 60%)"/>`
        + '<circle cx="92" cy="20" r="4.5" fill="#1b222d"/>'
        + '<path d="M106 26 L124 31 L106 36 Z" fill="#f7931a"/>'
        + '</g>'
        + '<path d="M18 206 Q90 186 160 204 T304 200 M30 222 Q110 200 190 220 T300 216 M12 232 Q120 212 220 232" stroke="#8a6a3e" stroke-width="3" fill="none"/>'
        + '<ellipse cx="160" cy="236" rx="160" ry="22" fill="#3d2c1c"/>'
        + `<text x="10" y="18" font-family="sans-serif" font-size="12" fill="#eef3f8">Visit ${visit.id} · ${label} UTC</text>`
        + '</svg>';
};

// Start the birdhouse again from a new device: default settings, and the network forgotten (the visits stay on the card, and the numbers keep counting)
export const resetBirdhouseSettings = state => {
    state.birdhouse.settings = defaultSettings();
    state.savedSsid = '';
};
