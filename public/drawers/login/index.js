var passwordInput = document.querySelector("#password");
var clearIconPassword = document.querySelector("#clear-icon-password");

passwordInput.addEventListener("input", () => {
    clearIconPassword.style.visibility = passwordInput.value ? "visible" : "hidden";
});

clearIconPassword.addEventListener("click", () => {
    passwordInput.value = "";
    clearIconPassword.style.visibility = "hidden";
});

if (!openLoginDrawer) {
    function openLoginDrawer() {
        const loginDrawer = document.getElementById("login_drawer");
        loginDrawer.showModal();
        requestAnimationFrame(() => {
            loginDrawer.classList.remove("translate-x-full");
        });
    }
}

if (!closeLoginDrawer) {
    function closeLoginDrawer() {
        const loginDrawer = document.getElementById("login_drawer");
        loginDrawer.classList.add("translate-x-full");
        loginDrawer.addEventListener(
            "transitionend",
            () => {
                loginDrawer.close();
            },
            { once: true }
        );
    }
}

var closeLoginDrawerBtn = document.getElementById("closeLoginDrawer");
closeLoginDrawerBtn.addEventListener("click", closeLoginDrawer);

openLoginDrawer();
