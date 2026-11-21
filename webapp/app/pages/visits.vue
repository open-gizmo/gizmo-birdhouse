<script setup>
import { computed, onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import BirdIcon from '@hugeicons/core-free-icons/BirdIcon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import Clock01Icon from '@hugeicons/core-free-icons/Clock01Icon';
import Delete02Icon from '@hugeicons/core-free-icons/Delete02Icon';
import Download04Icon from '@hugeicons/core-free-icons/Download04Icon';
import Radar01Icon from '@hugeicons/core-free-icons/Radar01Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import SdCardIcon from '@hugeicons/core-free-icons/SdCardIcon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { showConfirmDialog } from '@brumaombra/ui-vintage/confirm-dialog';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadMoreButton } from '@brumaombra/ui-vintage/load-more-button';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { loadVisits, removeAllVisits, removeVisit } from '~/composables/useBirdhouse.js';
import { buildVisitPhotoUrl } from '~/composables/useDeviceApi.js';
import { buildVisitFileName, formatFileSize, formatStorageSize, formatVisitTime } from '~/composables/useUtils.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const globalStore = useGlobalStore();
const { t, locale } = useI18n();
const isLoadingMore = ref(false);
const openVisit = ref(null); // Visit shown in the photo dialog, or null

// What the birdhouse is doing, and the visits read so far
const status = computed(() => globalStore.value.status);
const visits = computed(() => globalStore.value.visits);
const hasMoreVisits = computed(() => visits.value.length < globalStore.value.visitTotal);
const isStorageAvailable = computed(() => Boolean(status.value?.storage?.available));

// State of the detection, in a word: watching, off, or paused for a missing card
const detectionLabel = computed(() => {
    if (!status.value?.detection?.enabled) return t('pages.visits.detection.off');
    if (!isStorageAvailable.value) return t('pages.visits.detection.noCard');
    return status.value.detection.active ? t('pages.visits.detection.watching') : t('pages.visits.detection.paused');
});

// Space used on the card ("1.2 GB of 29.7 GB")
const storageLabel = computed(() => {
    if (!isStorageAvailable.value) return t('pages.visits.storage.missing');
    const used = formatStorageSize({ sizeBytes: status.value.storage.usedBytes, locale: locale.value });
    const total = formatStorageSize({ sizeBytes: status.value.storage.totalBytes, locale: locale.value });
    return t('pages.visits.storage.used', { used, total });
});

// Time of the newest visit
const lastVisitLabel = computed(() => {
    const lastVisit = status.value?.lastVisit;
    if (!lastVisit) return t('pages.visits.lastVisit.none');
    return describeVisitTime(lastVisit);
});

// Help under the status: how many visits, and a word on the clock when it isn't set
const statusHint = computed(() => {
    const count = t('pages.visits.count', { count: globalStore.value.visitTotal }, globalStore.value.visitTotal);
    return status.value?.clockSet === false ? `${count} ${t('pages.visits.clockNotSet')}` : count;
});

// Time of a visit in the phone's time zone, or its number when the device's clock wasn't set
const describeVisitTime = visit => formatVisitTime({ timestamp: visit.timestamp, locale: locale.value }) || t('pages.visits.unknownTime', { id: visit.id });

// Details of the visit in the dialog: its time and the size of its photo
const openVisitDetails = computed(() => {
    if (!openVisit.value) return '';
    const size = formatFileSize({ sizeBytes: openVisit.value.sizeBytes, locale: locale.value });
    return t('pages.visits.photoDetails', { time: describeVisitTime(openVisit.value), size });
});

// Read the visits and the status, showing the busy indicator meanwhile
const readVisits = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await loadVisits();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Read the next page of visits
const handleLoadMore = async () => {
    try {
        isLoadingMore.value = true;
        await loadVisits({ more: true });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        isLoadingMore.value = false;
    }
};

// Save the photo of the open visit (the browser downloads it from the device under the given name)
const handleSavePress = () => {
    if (!openVisit.value) return;
    const link = document.createElement('a');
    link.href = buildVisitPhotoUrl({ id: openVisit.value.id });
    link.download = buildVisitFileName(openVisit.value);
    link.click();
};

// Remove the open visit, then close its dialog
const deleteOpenVisit = async () => {
    const visit = openVisit.value;
    if (!visit) return;
    try {
        setBusy(true);
        await removeVisit(visit.id);
        openVisit.value = null;
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.visits.deleteError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Ask before removing the open visit
const handleDeletePress = () => {
    showConfirmDialog({
        icon: Delete02Icon,
        title: t('pages.visits.deleteConfirmTitle'),
        message: t('pages.visits.deleteConfirmMessage'),
        confirmText: t('pages.visits.deleteAction'),
        cancelText: t('common.cancel'),
        confirmButtonType: 'red',
        cancelButtonType: 'secondary',
        confirmButtonIcon: Delete02Icon,
        cancelButtonIcon: Cancel01Icon,
        onConfirm: deleteOpenVisit
    });
};

// Remove every visit
const deleteEveryVisit = async () => {
    try {
        setBusy(true);
        await removeAllVisits();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.visits.deleteError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Ask before removing every visit
const handleDeleteAllPress = () => {
    showConfirmDialog({
        icon: Delete02Icon,
        title: t('pages.visits.deleteAllConfirmTitle'),
        message: t('pages.visits.deleteAllConfirmMessage', { count: globalStore.value.visitTotal }, globalStore.value.visitTotal),
        confirmText: t('pages.visits.deleteAllAction'),
        cancelText: t('common.cancel'),
        confirmButtonType: 'red',
        cancelButtonType: 'secondary',
        confirmButtonIcon: Delete02Icon,
        cancelButtonIcon: Cancel01Icon,
        onConfirm: deleteEveryVisit
    });
};

// Read the visits each time the page opens (a bird may have come by meanwhile)
onMounted(readVisits);

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.visits.title')">
        <!-- The visits couldn't be read: offer to try again -->
        <EmptyStateCard v-if="globalStore.visitsLoadFailed && !globalStore.visitsLoaded" :icon="Alert02Icon" :title="t('pages.visits.loadErrorTitle')" :description="t('pages.visits.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="readVisits">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.visits.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the visits from the device -->
        <LoadingStateCard v-else-if="!globalStore.visitsLoaded" :title="t('common.syncingDevice')" />

        <template v-else>
            <!-- What the birdhouse is doing -->
            <ListGroup :title="t('pages.visits.statusTitle')" :hint="statusHint">
                <ListRow :label="t('pages.visits.detection.label')" :icon="Radar01Icon" :value="detectionLabel" to="/detection" />
                <ListRow :label="t('pages.visits.lastVisit.label')" :icon="Clock01Icon" :value="lastVisitLabel" />
                <ListRow :label="t('pages.visits.storage.label')" :icon="SdCardIcon" :value="storageLabel" wrap-value />
            </ListGroup>

            <!-- No card: the visits can't be saved -->
            <EmptyStateCard v-if="!isStorageAvailable" :icon="SdCardIcon" :title="t('pages.visits.noCardTitle')" :description="t('pages.visits.noCardDescription')" />

            <!-- No visit yet -->
            <EmptyStateCard v-else-if="visits.length === 0" :icon="BirdIcon" :title="t('pages.visits.emptyTitle')" :description="t('pages.visits.emptyDescription')" />

            <!-- The visits, newest first: the small picture of each, taken the moment the bird was seen -->
            <template v-else>
                <ul class="grid grid-cols-2 gap-3 sm:grid-cols-3" :aria-label="t('pages.visits.listLabel')">
                    <li v-for="visit in visits" :key="visit.id">
                        <button type="button" class="group block w-full overflow-hidden rounded-xl border border-border bg-card text-left shadow-elevated-sm transition-[translate,box-shadow] ease-spring hover:-translate-y-0.5 hover:shadow-elevated-md focus-visible:outline-2 focus-visible:outline-primary" @click="openVisit = visit">
                            <img :src="buildVisitPhotoUrl({ id: visit.id, isThumbnail: true })" :alt="t('pages.visits.thumbnailAlt', { time: describeVisitTime(visit) })" loading="lazy" decoding="async" width="320" height="240" class="block aspect-4/3 h-auto w-full bg-muted object-cover">
                            <span class="block truncate px-3 py-2 text-xs text-muted-foreground">{{ describeVisitTime(visit) }}</span>
                        </button>
                    </li>
                </ul>
                <LoadMoreButton v-if="hasMoreVisits" :busy="isLoadingMore" :text="t('pages.visits.loadMore')" @load-more="handleLoadMore" />
            </template>

            <!-- Remove every visit -->
            <ListGroup v-if="isStorageAvailable && globalStore.visitTotal > 0">
                <ListRow :label="t('pages.visits.deleteAllAction')" :icon="Delete02Icon" action danger @click="handleDeleteAllPress" />
            </ListGroup>
        </template>

        <!-- Photo of a visit -->
        <ResponsiveDialog :open="openVisit !== null" :title="openVisit ? t('pages.visits.photoTitle', { id: openVisit.id }) : ''" :description="openVisitDetails" size="lg" @update:open="open => { if (!open) openVisit = null; }">
            <img v-if="openVisit" :src="buildVisitPhotoUrl({ id: openVisit.id })" :alt="t('pages.visits.photoAlt', { time: describeVisitTime(openVisit) })" class="block h-auto w-full rounded-lg bg-muted">
            <template #footer>
                <Button variant="primary" @click="handleSavePress">
                    <HugeiconsIcon :icon="Download04Icon" class="size-4" />
                    {{ t('pages.visits.saveAction') }}
                </Button>
                <Button variant="secondary" @click="handleDeletePress">
                    <HugeiconsIcon :icon="Delete02Icon" class="size-4" />
                    {{ t('pages.visits.deleteAction') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </GizmoPage>
</template>
