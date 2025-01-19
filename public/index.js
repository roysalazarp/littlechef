document.addEventListener("DOMContentLoaded", () => {
    const navLinks = document.querySelectorAll("a.nav-link");
    const currentPath = window.location.pathname;

    navLinks.forEach((link) => {
        if (link.getAttribute("href") === currentPath) {
            link.classList.add("text-[#3EFFA2]");
            link.classList.remove("text-[#9D9DAE]");
        }
    });

    const openModalButton = document.querySelector("#openModal");
    const dialog = document.querySelector("#modal");
    const closeButton = document.querySelector("#closeModal");

    let isDragging = false;
    let startY = 0;
    let startTransform = 0;
    let currentTransform = 0;
    const maxUpwardDrag = -50;

    function handleDragStart(event) {
        if (!dialog.hasAttribute("open")) return;
        const touch = event.touches ? event.touches[0] : event;
        isDragging = true;
        startY = touch.clientY;
        const transform = new WebKitCSSMatrix(window.getComputedStyle(dialog).transform);
        startTransform = transform.m42;
        dialog.style.transition = "none";
    }

    function handleDragMove(event) {
        if (!isDragging) return;
        const touch = event.touches ? event.touches[0] : event;
        const deltaY = touch.clientY - startY;

        /* Only allow closing on downward drag */
        if (deltaY < 0) {
            /* Upward drag with resistance */
            const resistance = 0.3;
            currentTransform = Math.max(maxUpwardDrag, deltaY * resistance + startTransform);
        } else {
            /* Downward drag */
            currentTransform = deltaY + startTransform;
        }

        dialog.style.transform = `translateY(${currentTransform}px)`;

        /* Only update backdrop opacity on downward drag */
        if (deltaY > 0) {
            const backdrop = dialog.previousElementSibling;
            if (backdrop && backdrop.matches("::backdrop")) {
                const opacity = Math.max(0, 1 - currentTransform / (window.innerHeight * 0.7));
                backdrop.style.backgroundColor = `rgba(0, 0, 0, ${opacity * 0.7})`;
            }
        }
    }

    function handleDragEnd() {
        if (!isDragging) return;
        isDragging = false;
        dialog.style.transition = "transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)";

        const threshold = window.innerHeight * 0.1;
        /* Only close on downward drag */
        if (currentTransform > threshold && currentTransform > 0) {
            closeModal();
        } else {
            /* Reset position with a small bounce effect */
            dialog.style.transform = "translateY(0)";
            const backdrop = dialog.previousElementSibling;
            if (backdrop && backdrop.matches("::backdrop")) {
                backdrop.style.backgroundColor = "rgba(0, 0, 0, 0.7)";
            }
        }
    }

    /* Rest of the JavaScript remains the same */
    function resetModalPosition() {
        dialog.style.transform = "translateY(0)";
        currentTransform = 0;
        startTransform = 0;
        isDragging = false;
    }

    async function startViewTransition(callback) {
        if (!document.startViewTransition) {
            callback();
            return;
        }

        const transition = document.startViewTransition(callback);
        try {
            await transition.finished;
            resetModalPosition();
        } catch (error) {
            resetModalPosition();
        }
    }

    openModalButton.addEventListener("click", () => {
        resetModalPosition();
        startViewTransition(() => {
            dialog.showModal();
        });
    });

    const closeModal = () => {
        startViewTransition(() => {
            dialog.close();
        });
    };

    if (dialog) {
        /* Touch events */
        dialog.addEventListener("touchstart", handleDragStart, {
            passive: true,
        });
        dialog.addEventListener("touchmove", handleDragMove, { passive: true });
        dialog.addEventListener("touchend", handleDragEnd);
        dialog.addEventListener("touchcancel", handleDragEnd);

        /* Mouse events */
        dialog.addEventListener("mousedown", handleDragStart);
        document.addEventListener("mousemove", handleDragMove);
        document.addEventListener("mouseup", handleDragEnd);

        /* Prevent dragging from starting when clicking buttons or links */
        /*
        dialog.addEventListener("mousedown", (event) => {
            if (event.target.tagName === "BUTTON" || event.target.tagName === "A") {
                isDragging = false;
            }
        });
        */

        dialog.addEventListener("click", (event) => {
            if (event.target === dialog) {
                closeModal();
            }
        });

        closeButton.addEventListener("click", closeModal);
        dialog.addEventListener("close", resetModalPosition);
    }
});
