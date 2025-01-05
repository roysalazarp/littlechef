if ("serviceWorker" in navigator) {
    navigator.serviceWorker
        .register("assets/public/sw.js")
        .then(() => console.log("Service worker registered"))
        .catch(() => console.log("Failed to registered service worker"));
}
