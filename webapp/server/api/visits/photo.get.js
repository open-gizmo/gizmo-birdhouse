import { getQuery, setResponseHeader } from 'h3';
import { EspMockError, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildVisitPhoto, findVisit, getEspMockState, parseWholeNumber } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Read the visit number with the firmware rules
        const query = getQuery(event);
        const id = query.id === undefined ? undefined : parseWholeNumber(query.id);
        if (!id) {
            throw new EspMockError(400, 'invalid_visit_id', 'id must be a visit number');
        }

        // Find the visit, then send its photo or its small picture (an SVG stands in for the JPEG of the card)
        const visit = findVisit({ state: getEspMockState(), id });
        setResponseHeader(event, 'Content-Type', 'image/svg+xml');
        setResponseHeader(event, 'Cache-Control', 'private, max-age=31536000, immutable');
        return buildVisitPhoto({ visit, isThumbnail: query.thumbnail === '1' });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'visit_not_found', errorMessage: 'Visit not found' });
    }
});
