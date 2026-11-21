import { EspMockError, createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Without a card there is nothing to remove
        const { birdhouse } = getEspMockState();
        if (!birdhouse.storage.available) {
            throw new EspMockError(503, 'storage_unavailable', 'No microSD card');
        }

        // The firmware refuses while the files of an earlier removal are still on the card
        if (birdhouse.isDeleteAllPending) {
            throw new EspMockError(409, 'visits_busy', 'The deleted visits are still being removed');
        }

        // Remove every visit (the numbers keep counting)
        birdhouse.visits = [];
        return createSuccessResponse();
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'visit_delete_failed', errorMessage: 'Failed to delete the visit' });
    }
});
