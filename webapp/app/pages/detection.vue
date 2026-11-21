<script setup>
import { computed, onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import AspectRatioIcon from '@hugeicons/core-free-icons/AspectRatioIcon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import FlashIcon from '@hugeicons/core-free-icons/FlashIcon';
import Radar01Icon from '@hugeicons/core-free-icons/Radar01Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import SaveIcon from '@hugeicons/core-free-icons/SaveIcon';
import Timer02Icon from '@hugeicons/core-free-icons/Timer02Icon';
import ViewIcon from '@hugeicons/core-free-icons/ViewIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { showConfirmDialog } from '@brumaombra/ui-vintage/confirm-dialog';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { OptionPickerDialog } from '@brumaombra/ui-vintage/option-picker-dialog';
import { Switch } from '@brumaombra/ui-vintage/switch';
import { loadSettings, storeSettings } from '~/composables/useBirdhouse.js';
import { loadCamera } from '~/composables/useCamera.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const globalStore = useGlobalStore();
const { t } = useI18n();
const openPicker = ref(''); // Picker currently open ('sensitivity', 'cooldown', 'resolution', or none)

// Settings of the detection, as read from the device
const settings = computed(() => globalStore.value.settings);

// Whether the page has what it needs: the settings, and the camera for the resolutions
const isLoaded = computed(() => globalStore.value.settingsLoaded && globalStore.value.cameraLoaded);
const hasLoadFailed = computed(() => (globalStore.value.settingsLoadFailed || globalStore.value.cameraLoadFailed) && !isLoaded.value);

// Sensitivities, from the one that needs a big change to the one that sees a small one
const sensitivityOptions = computed(() => ['low', 'medium', 'high'].map(value => ({
    value,
    label: t(`pages.detection.sensitivity.${value}`),
    description: t(`pages.detection.sensitivity.${value}Meta`)
})));

// Waits between two visit photos (the firmware takes 5 s to 10 min)
const cooldownOptions = computed(() => [10, 30, 60, 120, 300, 600].map(value => ({
    value,
    label: formatCooldown(value)
})));

// Resolutions the sensor can take, largest first (the sharpest is the one people look for)
const resolutionOptions = computed(() => [...globalStore.value.camera.resolutions].reverse().map(resolution => ({
    value: resolution.name,
    label: resolution.name,
    description: `${resolution.width} × ${resolution.height}`
})));

// Labels of the chosen values, shown in their rows
const selectedSensitivityLabel = computed(() => sensitivityOptions.value.find(option => option.value === settings.value.sensitivity)?.label || '');
const selectedCooldownLabel = computed(() => formatCooldown(settings.value.cooldownSeconds));

// A wait in seconds or minutes ("30 s", "2 min")
function formatCooldown(seconds) {
    if (seconds < 60 || seconds % 60 !== 0) return t('pages.detection.cooldown.seconds', { count: seconds });
    return t('pages.detection.cooldown.minutes', { count: seconds / 60 });
}

// Read the settings and the camera, showing the busy indicator meanwhile
const readSettings = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await Promise.all([
            loadSettings(),
            globalStore.value.cameraLoaded ? Promise.resolve() : loadCamera()
        ]);
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Save the settings
const saveCurrentSettings = async () => {
    // Only save the settings read from the device (never the store defaults)
    if (!isLoaded.value) return;

    try {
        setBusy(true);
        await storeSettings();
        showMessageDialog({
            type: 'success',
            icon: CheckmarkCircle02Icon,
            title: t('dialogs.successTitle'),
            message: t('pages.detection.saveSuccess'),
            closeText: t('common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.detection.saveError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Ask before saving the settings
const handleSavePress = () => {
    showConfirmDialog({
        icon: SaveIcon,
        title: t('pages.detection.saveConfirmTitle'),
        message: t('pages.detection.saveConfirmMessage'),
        confirmText: t('pages.detection.saveAction'),
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

// Read the settings each time the page opens (the form starts from what the device has)
onMounted(readSettings);

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.detection.title')">
        <!-- The settings couldn't be read: offer to try again (a form here would save the defaults over the real settings) -->
        <EmptyStateCard v-if="hasLoadFailed" :icon="Alert02Icon" :title="t('pages.detection.loadErrorTitle')" :description="t('pages.detection.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="readSettings">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.detection.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the settings from the device -->
        <LoadingStateCard v-else-if="!isLoaded" :title="t('common.syncingDevice')" />

        <!-- Settings form -->
        <form v-else class="space-y-6" @submit.prevent="handleSavePress">
            <!-- When a visit is seen -->
            <ListGroup :title="t('pages.detection.watchTitle')" :hint="t('pages.detection.watchHint')">
                <ListRow :label="t('pages.detection.enabled.label')" :description="t('pages.detection.enabled.description')" :icon="Radar01Icon" label-for="detection-enabled">
                    <template #trailing>
                        <Switch id="detection-enabled" v-model="settings.detectionEnabled" />
                    </template>
                </ListRow>
                <ListRow :label="t('pages.detection.sensitivity.label')" :icon="ViewIcon" :value="selectedSensitivityLabel" action @click="openPicker = 'sensitivity'" />
                <ListRow :label="t('pages.detection.cooldown.label')" :icon="Timer02Icon" :value="selectedCooldownLabel" action @click="openPicker = 'cooldown'" />
            </ListGroup>

            <!-- The visit photos -->
            <ListGroup :title="t('pages.detection.photoTitle')" :hint="t('pages.detection.photoHint', { sensor: globalStore.camera.sensor })">
                <ListRow :label="t('pages.detection.resolution.label')" :icon="AspectRatioIcon" :value="settings.photoResolution" action @click="openPicker = 'resolution'" />
                <ListRow :label="t('pages.detection.flash.label')" :description="t('pages.detection.flash.description')" :icon="FlashIcon" label-for="detection-flash">
                    <template #trailing>
                        <Switch id="detection-flash" v-model="settings.flash" />
                    </template>
                </ListRow>
            </ListGroup>

            <!-- Save button -->
            <Button variant="primary" type="submit" class="w-full">
                <HugeiconsIcon :icon="SaveIcon" class="size-4" />
                {{ t('pages.detection.saveAction') }}
            </Button>
        </form>

        <!-- Sensitivity picker -->
        <OptionPickerDialog :open="openPicker === 'sensitivity'" :title="t('pages.detection.sensitivity.label')" :description="t('pages.detection.sensitivity.note')" :options="sensitivityOptions" :model-value="settings.sensitivity" @update:model-value="value => settings.sensitivity = value" @update:open="handlePickerOpenUpdate" />

        <!-- Cooldown picker -->
        <OptionPickerDialog :open="openPicker === 'cooldown'" :title="t('pages.detection.cooldown.label')" :description="t('pages.detection.cooldown.note')" :options="cooldownOptions" :model-value="settings.cooldownSeconds" @update:model-value="value => settings.cooldownSeconds = value" @update:open="handlePickerOpenUpdate" />

        <!-- Photo resolution picker -->
        <OptionPickerDialog :open="openPicker === 'resolution'" :title="t('pages.detection.resolution.label')" :description="t('pages.detection.resolution.note')" :options="resolutionOptions" :model-value="settings.photoResolution" @update:model-value="value => settings.photoResolution = value" @update:open="handlePickerOpenUpdate" />
    </GizmoPage>
</template>
