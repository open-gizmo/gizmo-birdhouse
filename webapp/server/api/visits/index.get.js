import { getQuery } from 'h3';
import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildVisitsPayload, getEspMockState, parseVisitPage } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Read the page with the firmware rules
        const { offset, limit } = parseVisitPage(getQuery(event));

        // Return the visits of the page, newest first, like the firmware
        return createSuccessResponse(buildVisitsPayload({ state: getEspMockState(), offset, limit }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_visits_read_failed', errorMessage: 'Failed to read the visits' });
    }
});
