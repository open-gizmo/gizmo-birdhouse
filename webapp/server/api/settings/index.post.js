import { EspMockError, createSuccessResponse, handleEspMockError, readMockJsonBody } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { applySettings, findInvalidSettingsField, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Check the fields in the firmware order (a refused request changes nothing)
        const body = await readMockJsonBody(event);
        const state = getEspMockState();
        const invalidField = findInvalidSettingsField({ body, camera: state.camera });
        if (invalidField) {
            throw new EspMockError(400, 'invalid_settings_field', invalidField);
        }

        // Apply the fields present, and return the saved settings like the firmware
        applySettings({ settings: state.birdhouse.settings, body });
        return createSuccessResponse({ ...state.birdhouse.settings });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'settings_save_failed', errorMessage: 'Failed to save the settings' });
    }
});
