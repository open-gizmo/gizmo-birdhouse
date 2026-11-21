import BirdIcon from '@hugeicons/core-free-icons/BirdIcon';
import Camera01Icon from '@hugeicons/core-free-icons/Camera01Icon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';
import Radar01Icon from '@hugeicons/core-free-icons/Radar01Icon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Gizmo Birdhouse',

        // mDNS name of the firmware (http://gizmo-birdhouse.local)
        hostname: 'gizmo-birdhouse',

        logo: '/images/logo/gizmo-birdhouse-mark.svg',
        logoDark: '/images/logo/gizmo-birdhouse-mark-dark.svg',
        homePath: '/visits',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'visits', labelKey: 'nav.visits.label', descriptionKey: 'nav.visits.description', icon: BirdIcon, to: '/visits' },
            { id: 'live', labelKey: 'nav.live.label', descriptionKey: 'nav.live.description', icon: Camera01Icon, to: '/live' },
            { id: 'detection', labelKey: 'nav.detection.label', descriptionKey: 'nav.detection.description', icon: Radar01Icon, to: '/detection' },
            { id: 'device', labelKey: 'nav.device.label', descriptionKey: 'nav.device.description', icon: CpuIcon, to: '/device' },
            'wifi'
        ]
    }
});
