# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

The first version of Gizmo Birdhouse: an AI Thinker ESP32-CAM in a birdhouse that photographs the birds that come to visit.

- Visit detection with no extra wiring: the camera watches the birdhouse and takes a photo when the picture changes, with a low, medium or high sensitivity and a wait between photos (5 s to 1 h, 30 s by default) so a bird that stays, like one sitting on its nest, isn't photographed every second. Changes of light across the whole picture are left out.
- The photo of each visit, and a small picture of the moment the bird was seen, saved on the microSD card of the ESP32-CAM with the date and time (from the internet). When the card fills up, or after 1000 visits, the oldest visits make room for the new ones.
- A web app built into the firmware, in English and Italian: the **Visits** tab shows the visits newest first, with their photos to save or delete, the **Camera** tab takes a snapshot right now and sets how visits are photographed (sensitivity, wait, resolution, flash), the **Device** tab shows the device and resets its settings, and the **WiFi** tab changes the network.
- A small HTTP API for scripts and home automation: `/api/status`, `/api/visits`, `/api/visits/photo`, `/api/capture` and `/api/settings`.
- Wi-Fi setup from the phone, through the `Gizmo-Birdhouse` hotspot and its captive portal, then `gizmo-birdhouse.local` on your network.
