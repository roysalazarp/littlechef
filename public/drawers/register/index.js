var passwordInput = document.querySelector("#password");
var clearIconPassword = document.querySelector("#clear-icon-password");

passwordInput.addEventListener("input", () => {
    clearIconPassword.style.visibility = passwordInput.value ? "visible" : "hidden";
});

clearIconPassword.addEventListener("click", () => {
    passwordInput.value = "";
    clearIconPassword.style.visibility = "hidden";
});

var passwordAgainInput = document.querySelector("#password-again");
var clearIconPasswordAgain = document.querySelector("#clear-icon-password-again");

passwordAgainInput.addEventListener("input", () => {
    clearIconPasswordAgain.style.visibility = passwordAgainInput.value ? "visible" : "hidden";
});

clearIconPasswordAgain.addEventListener("click", () => {
    passwordAgainInput.value = "";
    clearIconPasswordAgain.style.visibility = "hidden";
});

var toggleSwitch = document.getElementById("terms-switch");
var hiddenInput = document.getElementById("terms-input");

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

if (!openRegisterDrawer) {
    function openRegisterDrawer() {
        const registerDrawer = document.getElementById("register_drawer");
        registerDrawer.showModal();
        requestAnimationFrame(() => {
            registerDrawer.classList.remove("translate-x-full");
        });
    }
}

if (!closeRegisterDrawer) {
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
}

var closeRegisterDrawerBtn = document.getElementById("closeRegisterDrawer");
closeRegisterDrawerBtn.addEventListener("click", closeRegisterDrawer);

openRegisterDrawer();
