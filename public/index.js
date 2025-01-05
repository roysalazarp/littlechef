document.addEventListener("DOMContentLoaded", () => {
    const navLinks = document.querySelectorAll("a.nav-link");
    const currentPath = window.location.pathname;

    navLinks.forEach((link) => {
        if (link.getAttribute("href") === currentPath) {
            link.classList.add("text-[#01B269]");
            link.classList.remove("text-gray-400");
        }
    });
});
