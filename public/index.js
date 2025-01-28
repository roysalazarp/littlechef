document.addEventListener("DOMContentLoaded", () => {
    const navLinks = document.querySelectorAll("a.nav-link");
    const currentPath = window.location.pathname;

    navLinks.forEach((link) => {
        if (link.getAttribute("href") === currentPath) {
            link.classList.add("text-[#3EFFA2]");
            link.classList.remove("text-[#9D9DAE]");
        }
    });

    const openBottomModalButton = document.querySelector("#openBottomModal");
    const bottomModal = document.querySelector("#bottomModal");
    const closeBottomModalButton = document.querySelector("#closeBottomModal");

    let isDragging = false;
    let startY = 0;
    let startTransform = 0;
    let currentTransform = 0;
    const maxUpwardDrag = -50;

    function handleDragStart(event) {
        if (!bottomModal.hasAttribute("open")) return;
        const touch = event.touches ? event.touches[0] : event;
        isDragging = true;
        startY = touch.clientY;
        const transform = new WebKitCSSMatrix(window.getComputedStyle(bottomModal).transform);
        startTransform = transform.m42;
        bottomModal.style.transition = "none";
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

        bottomModal.style.transform = `translateY(${currentTransform}px)`;

        /* Only update backdrop opacity on downward drag */
        if (deltaY > 0) {
            const backdrop = bottomModal.previousElementSibling;
            if (backdrop && backdrop.matches("::backdrop")) {
                const opacity = Math.max(0, 1 - currentTransform / (window.innerHeight * 0.7));
                backdrop.style.backgroundColor = `rgba(0, 0, 0, ${opacity * 0.7})`;
            }
        }
    }

    function handleDragEnd() {
        if (!isDragging) return;
        isDragging = false;
        bottomModal.style.transition = "transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)";

        const threshold = window.innerHeight * 0.1;
        /* Only close on downward drag */
        if (currentTransform > threshold && currentTransform > 0) {
            closeBottomModal();
        } else {
            /* Reset position with a small bounce effect */
            bottomModal.style.transform = "translateY(0)";
            const backdrop = bottomModal.previousElementSibling;
            if (backdrop && backdrop.matches("::backdrop")) {
                backdrop.style.backgroundColor = "rgba(0, 0, 0, 0.7)";
            }
        }
    }

    /* Rest of the JavaScript remains the same */
    function resetModalPosition() {
        bottomModal.style.transform = "translateY(0)";
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
        } catch (error) {
            console.error("Transition failed:", error);
        }
    }

    async function openSideView(view) {
        await startViewTransition(() => {
            view.showModal();
        });
    }

    async function closeSideView(view) {
        await startViewTransition(() => {
            view.close();
        });
    }

    const closeBottomModal = async () => {
        await startViewTransition(() => {
            bottomModal.close();
        });
    };

    if (bottomModal) {
        /* Touch events */
        bottomModal.addEventListener("touchstart", handleDragStart, {
            passive: true,
        });
        bottomModal.addEventListener("touchmove", handleDragMove, { passive: true });
        bottomModal.addEventListener("touchend", handleDragEnd);
        bottomModal.addEventListener("touchcancel", handleDragEnd);

        /* Mouse events */
        bottomModal.addEventListener("mousedown", handleDragStart);
        document.addEventListener("mousemove", handleDragMove);
        document.addEventListener("mouseup", handleDragEnd);

        bottomModal.addEventListener("click", (event) => {
            if (event.target === bottomModal) {
                closeBottomModal();
            }
        });

        openBottomModalButton.addEventListener("click", async () => {
            resetModalPosition();
            await startViewTransition(() => {
                bottomModal.showModal();
            });
        });

        closeBottomModalButton.addEventListener("click", closeBottomModal);
        bottomModal.addEventListener("close", resetModalPosition);
    }

    const dialogs = document.querySelectorAll("dialog");

    dialogs.forEach((dialog) => {
        const closeSideViewButtons = dialog.querySelectorAll(".close-side-view");

        const viewHeader = dialog.querySelector(".side-view-header");
        const stickyHeader = dialog.querySelector(".side-view-sticky-header");

        Array.from(closeSideViewButtons).forEach((btn) => {
            btn.addEventListener("click", async () => {
                if (stickyHeader.style.visibility === "visible") {
                    if (stickyHeader.classList.contains("-translate-y-full")) {
                        stickyHeader.style.visibility = "hidden";
                    }
                    await closeSideView(dialog);
                    stickyHeader.style.visibility = "hidden";
                } else {
                    await closeSideView(dialog);
                }
            });
        });

        dialog.addEventListener("scroll", (e) => {
            if (e.target.scrollTop > viewHeader.offsetHeight) {
                stickyHeader.classList.remove("-translate-y-full");
            } else {
                stickyHeader.classList.add("-translate-y-full");
            }
        });
    });

    document.querySelectorAll("[data-side-view-id]").forEach((openSideViewButton) => {
        const id = openSideViewButton.getAttribute("data-side-view-id");
        const dialog = Array.from(dialogs).find((dialog) => dialog.id === id);

        openSideViewButton.addEventListener("click", async () => {
            const stickyHeader = dialog.querySelector(".side-view-sticky-header");
            if (stickyHeader.style.visibility === "hidden") {
                await openSideView(dialog);
                stickyHeader.style.visibility = "visible";
            } else {
                stickyHeader.style.visibility = "hidden";
                await openSideView(dialog);
                stickyHeader.style.visibility = "visible";
            }
        });
    });

    document.querySelectorAll(".text-input").forEach((container) => {
        const input = container.querySelector("input");
        const clearButton = container.querySelector(".clear-button");

        if (input && clearButton) {
            input.addEventListener("input", () => {
                clearButton.style.visibility = input.value ? "visible" : "hidden";
            });

            clearButton.addEventListener("click", () => {
                input.value = "";
                clearButton.style.visibility = "hidden";
            });
        }
    });
});
