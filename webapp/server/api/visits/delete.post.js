import { EspMockError, createSuccessResponse, handleEspMockError, readMockJsonBody } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { findVisit, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Read the visit number like the firmware: a whole number from 1
        const body = await readMockJsonBody(event);
        if (!Number.isInteger(body.id) || body.id < 1 || body.id > 4294967295) {
            throw new EspMockError(400, 'invalid_visit_id', 'id must be a visit number');
        }

        // Remove the visit
        const state = getEspMockState();
        const visit = findVisit({ state, id: body.id });
        state.birdhouse.visits = state.birdhouse.visits.filter(candidate => candidate !== visit);
        return createSuccessResponse();
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'visit_delete_failed', errorMessage: 'Failed to delete the visit' });
    }
});
