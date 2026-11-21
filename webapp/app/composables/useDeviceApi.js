// The request wrapper (fetchEsp) and the Wi-Fi and device calls come from the gizmo-common layer: only the calls of the birdhouse are here

// Fetch what the camera can do: its sensor, resolutions, quality range and flash
export const getCamera = async () => {
    const response = await fetchEsp('/api/camera');
    return readData(response);
};

// Read the JSON error the device sends with a failed snapshot (the request asks for an image, so the error arrives as a blob that handleBackendErrors can't read)
const readSnapshotError = async error => {
    const errorBody = error?.response?._data;
    if (typeof errorBody?.text !== 'function') return; // Not a blob (checked by shape: a Blob from another realm fails instanceof)

    // Replace the blob with the device error, or with nothing when it isn't JSON
    try {
        const errorText = await errorBody.text();
        error.response._data = JSON.parse(errorText);
    } catch {
        error.response._data = null;
    }
};

// Take a snapshot, and return the image with its resolution and capture time (read from the response headers)
export const takeSnapshot = async ({ resolution, quality, flash }) => {
    let responseHeaders = null; // Set by the response hook, read once the image arrives

    try {
        // Call the device API to take the snapshot (never retried: each try is a new photo, and the flash would fire again)
        const image = await fetchEsp('/api/capture', {
            query: {
                resolution,
                quality,
                flash: flash ? 1 : 0
            },
            responseType: 'blob',
            retry: 0,
            onResponse: ({ response }) => {
                responseHeaders = response.headers;
            }
        });

        // Return the image and what the device says about it
        return {
            image,
            resolution: responseHeaders?.get('X-Resolution') || '',
            captureTimeMs: Number(responseHeaders?.get('X-Capture-Time-Ms')) || 0
        };
    } catch (error) {
        // Read the device error, so the dialog shows its translated message
        await readSnapshotError(error);
        throw error;
    }
};
// Fetch what the birdhouse is doing: the detection, the microSD card and the visits
export const getStatus = async () => {
    const response = await fetchEsp('/api/status');
    return readData(response);
};

// Fetch a page of visits, newest first
export const getVisits = async ({ offset = 0, limit = 20 } = {}) => {
    const response = await fetchEsp('/api/visits', { query: { offset, limit } });
    return readData(response);
};

// Address of the photo of a visit, or of its small picture, for an <img> (the device sends it from the card; it never changes, so the browser keeps it)
export const buildVisitPhotoUrl = ({ id, isThumbnail = false }) => {
    const baseUrl = (useRuntimeConfig().public.espBaseUrl || '').trim().replace(/\/$/, '');
    return `${baseUrl}/api/visits/photo?id=${id}${isThumbnail ? '&thumbnail=1' : ''}`;
};

// Remove a visit from the card
export const deleteVisit = async id => {
    await fetchEsp('/api/visits/delete', {
        method: 'POST',
        body: { id }
    });
    return true;
};

// Remove every visit from the card (POST, so other pages can't trigger it with a simple link)
export const deleteAllVisits = async () => {
    await fetchEsp('/api/visits/delete-all', { method: 'POST' });
    return true;
};

// Fetch the settings of the visit detection
export const getSettings = async () => {
    const response = await fetchEsp('/api/settings');
    return readData(response);
};

// Save the settings of the visit detection, and return them as the device saved them
export const saveSettings = async settings => {
    const response = await fetchEsp('/api/settings', {
        method: 'POST',
        body: settings
    });
    return readData(response);
};

// Reset the saved settings and the Wi-Fi, then restart the device (POST, so other pages can't trigger it with a simple link)
export const resetSettings = async () => {
    await fetchEsp('/api/reset-settings', { method: 'POST' });
    return true;
};
