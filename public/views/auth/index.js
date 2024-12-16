document.addEventListener("DOMContentLoaded", () => {
    const authDrawer = document.getElementById("auth_drawer");
    const openAuthDrawerBtn = document.getElementById("openAuthDrawer");
    const closeAuthDrawerBtn = document.getElementById("closeAuthDrawer");

    function openAuthDrawer() {
        authDrawer.showModal();
        requestAnimationFrame(() => {
            authDrawer.classList.remove("translate-x-full");
        });
    }

    function closeAuthDrawer() {
        authDrawer.classList.add("translate-x-full");
        authDrawer.addEventListener(
            "transitionend",
            () => {
                authDrawer.close();
            },
            { once: true }
        );
    }

    openAuthDrawerBtn.addEventListener("click", openAuthDrawer);
    closeAuthDrawerBtn.addEventListener("click", closeAuthDrawer);

    const emailInput = document.querySelector("#auth-email");
    const clearIcon = document.querySelector("#clear-icon");

    // Toggle visibility of "X" icon and enable/disable button
    emailInput.addEventListener("input", () => {
        clearIcon.style.visibility = emailInput.value ? "visible" : "hidden";
    });

    // Clear the email field when "X" icon is clicked
    clearIcon.addEventListener("click", () => {
        emailInput.value = "";
        clearIcon.style.visibility = "hidden";
    });
});
