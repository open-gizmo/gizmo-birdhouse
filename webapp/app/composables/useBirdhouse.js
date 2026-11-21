import { deleteAllVisits, deleteVisit, getSettings, getStatus, getVisits, saveSettings } from '~/composables/useDeviceApi.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Visits read per page
export const VISITS_PAGE_SIZE = 12;

// Read what the birdhouse is doing into the store
export const loadStatus = async () => {
    const globalStore = useGlobalStore();
    globalStore.value.status = await getStatus();
};

// Read the first page of visits (or the next one, with more), and the status with them
export const loadVisits = async ({ more = false } = {}) => {
    const globalStore = useGlobalStore();

    try {
        // Read the page after the visits already shown, or the first one
        const offset = more ? globalStore.value.visits.length : 0;
        const [page] = await Promise.all([
            getVisits({ offset, limit: VISITS_PAGE_SIZE }),
            more ? Promise.resolve() : loadStatus()
        ]);

        // Add the page, leaving out a visit already shown (a new visit moves the pages by one while reading)
        const knownIds = new Set(more ? globalStore.value.visits.map(visit => visit.id) : []);
        const visits = (page.visits || []).filter(visit => !knownIds.has(visit.id));
        globalStore.value.visits = more ? [...globalStore.value.visits, ...visits] : visits;
        globalStore.value.visitTotal = page.total ?? 0;
        globalStore.value.visitsLoaded = true;
        globalStore.value.visitsLoadFailed = false;
    } catch (error) {
        // Remember the failure, so the visits page offers to try again
        globalStore.value.visitsLoadFailed = true;
        throw error;
    }
};

// Remove a visit, and take it out of the list
export const removeVisit = async id => {
    const globalStore = useGlobalStore();
    await deleteVisit(id);
    globalStore.value.visits = globalStore.value.visits.filter(visit => visit.id !== id);
    globalStore.value.visitTotal = Math.max(0, globalStore.value.visitTotal - 1);
    if (globalStore.value.status) {
        globalStore.value.status.visitCount = globalStore.value.visitTotal;
        globalStore.value.status.lastVisit = globalStore.value.visits[0] ?? null;
    }
};

// Remove every visit, and empty the list
export const removeAllVisits = async () => {
    const globalStore = useGlobalStore();
    await deleteAllVisits();
    globalStore.value.visits = [];
    globalStore.value.visitTotal = 0;
    await loadStatus();
};

// Read the settings into the store, and remember a failure (so the camera page never saves the store defaults)
export const loadSettings = async () => {
    const globalStore = useGlobalStore();

    try {
        const settings = await getSettings();
        globalStore.value.settings = { ...globalStore.value.settings, ...settings };
        globalStore.value.settingsLoaded = true;
        globalStore.value.settingsLoadFailed = false;
    } catch (error) {
        globalStore.value.settingsLoadFailed = true;
        throw error;
    }
};

// Save the settings of the store, and keep what the device saved
export const storeSettings = async () => {
    const globalStore = useGlobalStore();
    const settings = await saveSettings(globalStore.value.settings);
    globalStore.value.settings = { ...globalStore.value.settings, ...settings };
};
