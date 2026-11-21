// Format a file size in KB (one decimal under 10 KB), in the language of the app
export const formatFileSize = ({ sizeBytes, locale }) => {
    const kilobytes = sizeBytes / 1024;
    const decimals = kilobytes < 10 ? 1 : 0;

    // Format the number in the current locale
    const formattedNumber = new Intl.NumberFormat(locale, {
        minimumFractionDigits: decimals,
        maximumFractionDigits: decimals
    }).format(kilobytes);

    // Return the size with its unit
    return `${formattedNumber} KB`;
};

// Format the space of the card in GB (one decimal), in the language of the app
export const formatStorageSize = ({ sizeBytes, locale }) => {
    const gigabytes = sizeBytes / (1024 * 1024 * 1024);
    const formattedNumber = new Intl.NumberFormat(locale, {
        minimumFractionDigits: 1,
        maximumFractionDigits: 1
    }).format(gigabytes);
    return `${formattedNumber} GB`;
};

// Format the time of a visit in the phone's own time zone (the device sends seconds since 1970, in UTC), or '' when the device's clock wasn't set
export const formatVisitTime = ({ timestamp, locale }) => {
    if (!timestamp) return '';
    return new Intl.DateTimeFormat(locale, { dateStyle: 'medium', timeStyle: 'short' }).format(new Date(timestamp * 1000));
};

// Pad a number to two digits for file names
const pad = value => String(value).padStart(2, '0');

// Build the file name of a snapshot from the time it was saved (snapshot-2026-11-09-14-30-05.jpg)
export const buildSnapshotFileName = (date = new Date()) => {
    const datePart = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
    const timePart = `${pad(date.getHours())}-${pad(date.getMinutes())}-${pad(date.getSeconds())}`;
    return `snapshot-${datePart}-${timePart}.jpg`;
};

// Build the file name of a visit photo from its number and time (visit-42-2026-11-09-14-30.jpg, or visit-42.jpg without a time)
export const buildVisitFileName = ({ id, timestamp }) => {
    if (!timestamp) return `visit-${id}.jpg`;
    const date = new Date(timestamp * 1000);
    return `visit-${id}-${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}-${pad(date.getHours())}-${pad(date.getMinutes())}.jpg`;
};
