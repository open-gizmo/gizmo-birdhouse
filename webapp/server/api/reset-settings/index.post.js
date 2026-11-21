import { createSuccessResponse, handleEspMockError, scheduleMockRestart } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { getEspMockState, resetBirdhouseSettings } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Clear the settings and the saved network, then restart with the hotspot open (the visits stay on the card)
        const state = getEspMockState();
        resetBirdhouseSettings(state);
        scheduleMockRestart(state);
        return createSuccessResponse();
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'clear_settings_failed', errorMessage: 'Failed to clear the settings' });
    }
});
