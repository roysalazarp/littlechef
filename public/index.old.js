document.addEventListener("DOMContentLoaded", () => {
    const navLinks = document.querySelectorAll("a.nav-link");
    const currentPath = window.location.pathname;

    navLinks.forEach((link) => {
        if (link.getAttribute("href") === currentPath) {
            link.classList.add("text-[#01B269]");
            link.classList.remove("text-gray-400");
        }
    });

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

    if (openAuthDrawerBtn) {
        openAuthDrawerBtn.addEventListener("click", openAuthDrawer);
    }

    if (closeAuthDrawerBtn) {
        closeAuthDrawerBtn.addEventListener("click", closeAuthDrawer);
    }

    const emailInput = document.querySelector("#auth-email");
    const clearIcon = document.querySelector("#clear-icon");

    /* Toggle visibility of "X" icon and enable/disable button */
    if (emailInput) {
        emailInput.addEventListener("input", () => {
            clearIcon.style.visibility = emailInput.value ? "visible" : "hidden";
        });
    }

    /* Clear the email field when "X" icon is clicked */
    if (clearIcon) {
        clearIcon.addEventListener("click", () => {
            emailInput.value = "";
            clearIcon.style.visibility = "hidden";
        });
    }

    var passwordInput = document.querySelector("#password");
    var clearIconPassword = document.querySelector("#clear-icon-password");

    if (passwordInput) {
        passwordInput.addEventListener("input", () => {
            clearIconPassword.style.visibility = passwordInput.value ? "visible" : "hidden";
        });
    }

    if (clearIconPassword) {
        clearIconPassword.addEventListener("click", () => {
            passwordInput.value = "";
            clearIconPassword.style.visibility = "hidden";
        });
    }

    var passwordAgainInput = document.querySelector("#password-again");
    var clearIconPasswordAgain = document.querySelector("#clear-icon-password-again");

    if (passwordAgainInput) {
        passwordAgainInput.addEventListener("input", () => {
            clearIconPasswordAgain.style.visibility = passwordAgainInput.value ? "visible" : "hidden";
        });
    }

    if (clearIconPasswordAgain) {
        clearIconPasswordAgain.addEventListener("click", () => {
            passwordAgainInput.value = "";
            clearIconPasswordAgain.style.visibility = "hidden";
        });
    }

    var toggleSwitch = document.getElementById("terms-switch");
    var hiddenInput = document.getElementById("terms-input");

    if (toggleSwitch) {
        toggleSwitch.addEventListener("click", function () {
            const isChecked = toggleSwitch.getAttribute("aria-checked") === "true";

            toggleSwitch.setAttribute("aria-checked", String(!isChecked));

            const switchButton = toggleSwitch.querySelector("span");
            if (isChecked) {
                switchButton.classList.remove("translate-x-5");
                switchButton.classList.add("translate-x-0");
                toggleSwitch.classList.remove("bg-blue-600");
                toggleSwitch.classList.add("bg-[#3C3C3C]");

                hiddenInput.value = "false";
            } else {
                switchButton.classList.remove("translate-x-0");
                switchButton.classList.add("translate-x-5");
                toggleSwitch.classList.remove("bg-[#3C3C3C]");
                toggleSwitch.classList.add("bg-blue-600");

                hiddenInput.value = "true";
            }
        });
    }

    function openRegisterDrawer() {
        const registerDrawer = document.getElementById("register_drawer");
        registerDrawer.showModal();
        requestAnimationFrame(() => {
            registerDrawer.classList.remove("translate-x-full");
        });
    }

    function closeRegisterDrawer() {
        const registerDrawer = document.getElementById("register_drawer");
        registerDrawer.classList.add("translate-x-full");
        registerDrawer.addEventListener(
            "transitionend",
            () => {
                registerDrawer.close();
            },
            { once: true }
        );
    }

    var closeRegisterDrawerBtn = document.getElementById("closeRegisterDrawer");
    if (closeRegisterDrawerBtn) {
        closeRegisterDrawerBtn.addEventListener("click", closeRegisterDrawer);
    }

    /* openRegisterDrawer(); */

    var passwordInput = document.querySelector("#password");
    var clearIconPassword = document.querySelector("#clear-icon-password");

    if (passwordInput) {
        passwordInput.addEventListener("input", () => {
            clearIconPassword.style.visibility = passwordInput.value ? "visible" : "hidden";
        });
    }

    if (clearIconPassword) {
        clearIconPassword.addEventListener("click", () => {
            passwordInput.value = "";
            clearIconPassword.style.visibility = "hidden";
        });
    }

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
    if (closeLoginDrawerBtn) {
        closeLoginDrawerBtn.addEventListener("click", closeLoginDrawer);
    }

    /* openLoginDrawer(); */
});
