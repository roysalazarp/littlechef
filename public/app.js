if ("serviceWorker" in navigator) {
    navigator.serviceWorker
        .register("/public/sw.js")
        .then(() => console.log("Service worker registered"))
        .catch(() => console.log("Failed to registered service worker"));
}
