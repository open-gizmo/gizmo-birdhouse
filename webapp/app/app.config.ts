import BirdIcon from '@hugeicons/core-free-icons/BirdIcon';
import Camera01Icon from '@hugeicons/core-free-icons/Camera01Icon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';

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
            { id: 'camera', labelKey: 'nav.camera.label', descriptionKey: 'nav.camera.description', icon: Camera01Icon, to: '/camera' },
            { id: 'device', labelKey: 'nav.device.label', descriptionKey: 'nav.device.description', icon: CpuIcon, to: '/device' },
            'wifi'
        ]
    }
});
