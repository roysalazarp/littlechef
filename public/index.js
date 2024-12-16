const disableInteractions = () => {
    document.body.style.pointerEvents = "none";
};

const enableInteractions = () => {
    document.body.style.pointerEvents = "auto";
};

document.addEventListener("DOMContentLoaded", () => {
    disableInteractions();

    const navLinks = document.querySelectorAll("a.nav-link");
    const transitionBackground = document.querySelector(".transition-bg");

    setTimeout(() => {
        transitionBackground.classList.remove("no-transition-bg");
        enableInteractions();
    }, 200);

    navLinks.forEach((link) => {
        link.addEventListener("click", (e) => {
            e.preventDefault();

            const target = link.getAttribute("href");

            transitionBackground.classList.add("no-transition-bg");

            setTimeout(() => {
                window.location.href = target;
            }, 200);
        });
    });

    const currentPath = window.location.pathname;

    navLinks.forEach((link) => {
        if (link.getAttribute("href") === currentPath) {
            link.classList.add("text-[#01B269]");
            link.classList.remove("text-gray-400");
        }
    });
});
