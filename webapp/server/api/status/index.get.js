import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildStatusPayload, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Return what the birdhouse is doing, like the firmware
        return createSuccessResponse(buildStatusPayload(getEspMockState()));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_status_read_failed', errorMessage: 'Failed to read the status' });
    }
});
