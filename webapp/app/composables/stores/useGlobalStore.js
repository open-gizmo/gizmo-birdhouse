// Shared device and UI state for the webapp
export const useGlobalStore = () => useState('global', () => ({
    // What the camera can do, as read from the device
    camera: {
        available: false,
        sensor: '',
        resolutions: [],
        defaultResolution: '',
        quality: {
            min: 4,
            max: 63,
            default: 10
        },
        hasFlash: false
    },

    // Flag to track whether the camera has been read at least once
    cameraLoaded: false,

    // Flag set when the camera couldn't be read (the live page then offers to try again)
    cameraLoadFailed: false,

    // Options of the next live snapshot (the resolution is set to the camera's default once it's read)
    snapshotOptions: {
        resolution: '',
        quality: 10,
        flash: false
    },

    // Last live snapshot taken: { url, resolution, sizeBytes, captureTimeMs }, or null
    snapshot: null,

    // What the birdhouse is doing (GET /api/status), or null before it's read
    status: null,

    // Visits read so far, newest first, and how many there are on the card
    visits: [],
    visitTotal: 0,
    visitsLoaded: false,
    visitsLoadFailed: false,

    // Settings of the visit detection, as read from the device
    settings: {
        detectionEnabled: true,
        sensitivity: 'medium',
        cooldownSeconds: 30,
        photoResolution: 'SXGA',
        flash: false
    },

    // Flag to track whether the settings have been read (the detection page never saves the defaults above)
    settingsLoaded: false,

    // Flag set when the settings couldn't be read (the detection page then offers to try again)
    settingsLoadFailed: false
}));
