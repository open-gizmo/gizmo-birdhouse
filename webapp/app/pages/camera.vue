<script setup>
import { computed, onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import AspectRatioIcon from '@hugeicons/core-free-icons/AspectRatioIcon';
import Camera01Icon from '@hugeicons/core-free-icons/Camera01Icon';
import CameraOff01Icon from '@hugeicons/core-free-icons/CameraOff01Icon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import Download04Icon from '@hugeicons/core-free-icons/Download04Icon';
import FlashIcon from '@hugeicons/core-free-icons/FlashIcon';
import Image01Icon from '@hugeicons/core-free-icons/Image01Icon';
import ImageCompositionIcon from '@hugeicons/core-free-icons/ImageCompositionIcon';
import Radar01Icon from '@hugeicons/core-free-icons/Radar01Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import SaveIcon from '@hugeicons/core-free-icons/SaveIcon';
import Timer02Icon from '@hugeicons/core-free-icons/Timer02Icon';
import ViewIcon from '@hugeicons/core-free-icons/ViewIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { Card } from '@brumaombra/ui-vintage/card';
import { showConfirmDialog } from '@brumaombra/ui-vintage/confirm-dialog';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { OptionPickerDialog } from '@brumaombra/ui-vintage/option-picker-dialog';
import { Switch } from '@brumaombra/ui-vintage/switch';
import { loadSettings, storeSettings } from '~/composables/useBirdhouse.js';
import { captureSnapshot, loadCamera } from '~/composables/useCamera.js';
import { buildSnapshotFileName, formatFileSize } from '~/composables/useUtils.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const globalStore = useGlobalStore();
const { t, locale } = useI18n();
const openPicker = ref(''); // Picker currently open ('snapshotResolution', 'quality', 'sensitivity', 'cooldown', 'photoResolution', or none)

// What the camera can do, the options of the next snapshot, and the last snapshot
const camera = computed(() => globalStore.value.camera);
const options = computed(() => globalStore.value.snapshotOptions);
const snapshot = computed(() => globalStore.value.snapshot);

// Settings of the detection, as read from the device
const settings = computed(() => globalStore.value.settings);

// Resolutions the sensor can take, largest first (the sharpest is the one people look for)
const resolutionOptions = computed(() => [...camera.value.resolutions].reverse().map(resolution => ({
    value: resolution.name,
    label: resolution.name,
    description: `${resolution.width} × ${resolution.height}`
})));

// Quality presets of the snapshot (the sensor's JPEG quality: lower numbers mean sharper photos and bigger files)
const qualityOptions = computed(() => [
    { value: 6, label: t('pages.camera.live.quality.best'), description: t('pages.camera.live.quality.bestMeta') },
    { value: 10, label: t('pages.camera.live.quality.high'), description: t('pages.camera.live.quality.highMeta') },
    { value: 20, label: t('pages.camera.live.quality.medium'), description: t('pages.camera.live.quality.mediumMeta') },
    { value: 40, label: t('pages.camera.live.quality.low'), description: t('pages.camera.live.quality.lowMeta') }
]);

// Sensitivities, from the one that needs a big change to the one that sees a small one
const sensitivityOptions = computed(() => ['low', 'medium', 'high'].map(value => ({
    value,
    label: t(`pages.camera.detection.sensitivity.${value}`),
    description: t(`pages.camera.detection.sensitivity.${value}Meta`)
})));

// Waits between two visit photos (the firmware takes 5 s to 1 h: the long ones are for a bird sitting on its nest)
const cooldownOptions = computed(() => [10, 30, 60, 120, 300, 600, 1800, 3600].map(value => ({
    value,
    label: formatCooldown(value)
})));

// Labels of the chosen values, shown in their rows (the number itself for a quality set outside the presets)
const selectedQualityLabel = computed(() => qualityOptions.value.find(option => option.value === options.value.quality)?.label || String(options.value.quality));
const selectedSensitivityLabel = computed(() => sensitivityOptions.value.find(option => option.value === settings.value.sensitivity)?.label || '');
const selectedCooldownLabel = computed(() => formatCooldown(settings.value.cooldownSeconds));

// Details of the last snapshot: resolution, size and capture time
const snapshotDetails = computed(() => {
    if (!snapshot.value) return '';
    const size = formatFileSize({ sizeBytes: snapshot.value.sizeBytes, locale: locale.value });
    return t('pages.camera.live.snapshotDetails', { resolution: snapshot.value.resolution, size, time: snapshot.value.captureTimeMs });
});

// A wait in seconds, minutes or hours ("30 s", "2 min", "1 h")
function formatCooldown(seconds) {
    if (seconds < 60 || seconds % 60 !== 0) return t('pages.camera.detection.cooldown.seconds', { count: seconds });
    if (seconds < 3600 || seconds % 3600 !== 0) return t('pages.camera.detection.cooldown.minutes', { count: seconds / 60 });
    return t('pages.camera.detection.cooldown.hours', { count: seconds / 3600 });
}

// Read the camera (once: the store keeps it across pages), and the settings each time the page opens (the form starts from what the device has)
const readDevice = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await Promise.all([
            globalStore.value.cameraLoaded ? Promise.resolve() : loadCamera(),
            loadSettings()
        ]);
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Take a snapshot with the chosen options
const handleSnapshotPress = async () => {
    try {
        setBusy(true, { label: t('pages.camera.live.takingSnapshot') });
        await captureSnapshot();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.camera.live.snapshotError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Save the last snapshot as a JPEG file
const handleDownloadPress = () => {
    // Nothing to save before the first snapshot
    if (!snapshot.value) return;

    // Click a temporary link to the image (the browser saves it under the given name)
    const link = document.createElement('a');
    link.href = snapshot.value.url;
    link.download = buildSnapshotFileName();
    link.click();
};

// Save the settings of the detection
const saveCurrentSettings = async () => {
    // Only save the settings read from the device (never the store defaults)
    if (!globalStore.value.settingsLoaded) return;

    try {
        setBusy(true);
        await storeSettings();
        showMessageDialog({
            type: 'success',
            icon: CheckmarkCircle02Icon,
            title: t('dialogs.successTitle'),
            message: t('pages.camera.detection.saveSuccess'),
            closeText: t('common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.camera.detection.saveError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Ask before saving the settings
const handleSavePress = () => {
    showConfirmDialog({
        icon: SaveIcon,
        title: t('pages.camera.detection.saveConfirmTitle'),
        message: t('pages.camera.detection.saveConfirmMessage'),
        confirmText: t('pages.camera.detection.saveAction'),
        cancelText: t('common.keepEditing'),
        confirmButtonType: 'primary',
        cancelButtonType: 'secondary',
        confirmButtonIcon: SaveIcon,
        cancelButtonIcon: Cancel01Icon,
        onConfirm: saveCurrentSettings
    });
};

// Close the open picker
const handlePickerOpenUpdate = open => {
    if (!open) openPicker.value = '';
};

// Read the camera and the settings when the page opens
onMounted(readDevice);

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.camera.title')">
        <!-- The camera couldn't be read: offer to try again -->
        <EmptyStateCard v-if="globalStore.cameraLoadFailed && !globalStore.cameraLoaded" :icon="Alert02Icon" :title="t('pages.camera.loadErrorTitle')" :description="t('pages.camera.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="readDevice">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.camera.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the camera from the device -->
        <LoadingStateCard v-else-if="!globalStore.cameraLoaded" :title="t('common.syncingDevice')" />

        <!-- The sensor didn't start on the device: no snapshot, and no visit to detect -->
        <EmptyStateCard v-else-if="!camera.available" :icon="CameraOff01Icon" :title="t('pages.camera.unavailableTitle')" :description="t('pages.camera.unavailableDescription')" />

        <template v-else>
            <!-- Last snapshot, or a placeholder before the first one (a live look inside the birdhouse: the visit photos are on the Visits page) -->
            <Card v-if="snapshot" padding="none" class="overflow-hidden">
                <img :src="snapshot.url" :alt="t('pages.camera.live.snapshotAlt')" class="block h-auto w-full bg-muted">
                <p class="px-4 py-3 text-xs text-muted-foreground">{{ snapshotDetails }}</p>
            </Card>
            <EmptyStateCard v-else :icon="Image01Icon" :title="t('pages.camera.live.emptyTitle')" :description="t('pages.camera.live.emptyDescription')" />

            <!-- Actions: take a snapshot, save the last one -->
            <div class="flex flex-col gap-3 sm:flex-row">
                <Button variant="primary" class="w-full sm:flex-1" @click="handleSnapshotPress">
                    <HugeiconsIcon :icon="Camera01Icon" class="size-4" />
                    {{ t('pages.camera.live.snapshotAction') }}
                </Button>
                <Button variant="secondary" class="w-full sm:flex-1" :disabled="!snapshot" @click="handleDownloadPress">
                    <HugeiconsIcon :icon="Download04Icon" class="size-4" />
                    {{ t('pages.camera.live.downloadAction') }}
                </Button>
            </div>

            <!-- Options of the next snapshot -->
            <ListGroup :title="t('pages.camera.live.optionsTitle')" :hint="t('pages.camera.live.optionsHint', { sensor: camera.sensor })">
                <ListRow :label="t('pages.camera.live.resolution.label')" :icon="AspectRatioIcon" :value="options.resolution" action @click="openPicker = 'snapshotResolution'" />
                <ListRow :label="t('pages.camera.live.quality.label')" :icon="ImageCompositionIcon" :value="selectedQualityLabel" action @click="openPicker = 'quality'" />
                <ListRow v-if="camera.hasFlash" :label="t('pages.camera.live.flash.label')" :description="t('pages.camera.live.flash.description')" :icon="FlashIcon" label-for="snapshot-flash">
                    <template #trailing>
                        <Switch id="snapshot-flash" v-model="options.flash" />
                    </template>
                </ListRow>
            </ListGroup>

            <!-- The settings couldn't be read: offer to try again (a form here would save the defaults over the real settings) -->
            <EmptyStateCard v-if="globalStore.settingsLoadFailed && !globalStore.settingsLoaded" :icon="Alert02Icon" :title="t('pages.camera.detection.loadErrorTitle')" :description="t('pages.camera.detection.loadErrorDescription')">
                <template #action>
                    <Button variant="primary" @click="readDevice">
                        <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                        {{ t('pages.camera.loadRetryAction') }}
                    </Button>
                </template>
            </EmptyStateCard>

            <!-- Still reading the settings from the device -->
            <LoadingStateCard v-else-if="!globalStore.settingsLoaded" :title="t('common.syncingDevice')" />

            <!-- Settings of the visit detection -->
            <form v-else class="space-y-6" @submit.prevent="handleSavePress">
                <!-- When a visit is seen -->
                <ListGroup :title="t('pages.camera.detection.watchTitle')" :hint="t('pages.camera.detection.watchHint')">
                    <ListRow :label="t('pages.camera.detection.enabled.label')" :description="t('pages.camera.detection.enabled.description')" :icon="Radar01Icon" label-for="detection-enabled">
                        <template #trailing>
                            <Switch id="detection-enabled" v-model="settings.detectionEnabled" />
                        </template>
                    </ListRow>
                    <ListRow :label="t('pages.camera.detection.sensitivity.label')" :icon="ViewIcon" :value="selectedSensitivityLabel" action @click="openPicker = 'sensitivity'" />
                    <ListRow :label="t('pages.camera.detection.cooldown.label')" :icon="Timer02Icon" :value="selectedCooldownLabel" action @click="openPicker = 'cooldown'" />
                </ListGroup>

                <!-- The visit photos -->
                <ListGroup :title="t('pages.camera.detection.photoTitle')" :hint="t('pages.camera.detection.photoHint')">
                    <ListRow :label="t('pages.camera.detection.resolution.label')" :icon="AspectRatioIcon" :value="settings.photoResolution" action @click="openPicker = 'photoResolution'" />
                    <ListRow :label="t('pages.camera.detection.flash.label')" :description="t('pages.camera.detection.flash.description')" :icon="FlashIcon" label-for="detection-flash">
                        <template #trailing>
                            <Switch id="detection-flash" v-model="settings.flash" />
                        </template>
                    </ListRow>
                </ListGroup>

                <!-- Save button -->
                <Button variant="primary" type="submit" class="w-full">
                    <HugeiconsIcon :icon="SaveIcon" class="size-4" />
                    {{ t('pages.camera.detection.saveAction') }}
                </Button>
            </form>
        </template>

        <!-- Snapshot resolution picker -->
        <OptionPickerDialog :open="openPicker === 'snapshotResolution'" :title="t('pages.camera.live.resolution.label')" :description="t('pages.camera.live.resolution.note')" :options="resolutionOptions" :model-value="options.resolution" @update:model-value="value => options.resolution = value" @update:open="handlePickerOpenUpdate" />

        <!-- Snapshot quality picker -->
        <OptionPickerDialog :open="openPicker === 'quality'" :title="t('pages.camera.live.quality.label')" :description="t('pages.camera.live.quality.note')" :options="qualityOptions" :model-value="options.quality" @update:model-value="value => options.quality = value" @update:open="handlePickerOpenUpdate" />

        <!-- Sensitivity picker -->
        <OptionPickerDialog :open="openPicker === 'sensitivity'" :title="t('pages.camera.detection.sensitivity.label')" :description="t('pages.camera.detection.sensitivity.note')" :options="sensitivityOptions" :model-value="settings.sensitivity" @update:model-value="value => settings.sensitivity = value" @update:open="handlePickerOpenUpdate" />

        <!-- Cooldown picker -->
        <OptionPickerDialog :open="openPicker === 'cooldown'" :title="t('pages.camera.detection.cooldown.label')" :description="t('pages.camera.detection.cooldown.note')" :options="cooldownOptions" :model-value="settings.cooldownSeconds" @update:model-value="value => settings.cooldownSeconds = value" @update:open="handlePickerOpenUpdate" />

        <!-- Visit photo resolution picker -->
        <OptionPickerDialog :open="openPicker === 'photoResolution'" :title="t('pages.camera.detection.resolution.label')" :description="t('pages.camera.detection.resolution.note')" :options="resolutionOptions" :model-value="settings.photoResolution" @update:model-value="value => settings.photoResolution = value" @update:open="handlePickerOpenUpdate" />
    </GizmoPage>
</template>
