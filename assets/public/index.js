const navLinks = document.querySelectorAll("a.nav-link");
navLinks.forEach((link) => {
    if (link.getAttribute("href") === window.location.pathname) {
        link.classList.add("text-[#3EFFA2]");
        link.classList.remove("text-[#9D9DAE]");
    }
});

/**
 * @param {() => void} callback
 */
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

/** VIEWS */

/**
 * @param {Element} dialog
 */
async function transitionOpenView(dialog) {
    const parentModal = dialog.parentNode.closest("dialog");
    if (parentModal) {
        parentModal.style.viewTransitionName = "none";
    }
    dialog.style.viewTransitionName = "view-content";
    await startViewTransition(() => {
        dialog.showModal();
    });
}

/**
 * @param {Element} dialog
 */
async function transitionCloseView(dialog) {
    await startViewTransition(() => {
        dialog.close();
    });
    dialog.style.viewTransitionName = "none";
    const parentModal = dialog.parentNode.closest("dialog");
    if (parentModal) {
        parentModal.style.viewTransitionName = "view-content";
    }
}

/**
 * @param {Element} dialog
 */
async function openView(dialog) {
    const stickyHeader = dialog.querySelector(".view-sticky-header");
    if (stickyHeader.style.visibility === "hidden") {
        await transitionOpenView(dialog);
        stickyHeader.style.visibility = "visible";
    } else {
        stickyHeader.style.visibility = "hidden";
        await transitionOpenView(dialog);
        stickyHeader.style.visibility = "visible";
    }
}

/**
 * @param {Element} button
 * @param {Element} dialog
 */
async function openViewButtonSetup(button, dialog) {
    button.addEventListener("click", async () => {
        await openView(dialog);
    });
}

/** All view dialogs have 2 close buttons */
const VIEW_CLOSE_BUTTONS = 2;

/**
 * @param {Element} dialog
 */
async function viewSetup(dialog) {
    const closeButtons = dialog.querySelectorAll(".close-view");

    const header = dialog.querySelector(".view-header");
    const stickyHeader = dialog.querySelector(".view-sticky-header");

    if (closeButtons.length >= VIEW_CLOSE_BUTTONS) {
        for (let i = 0; i < VIEW_CLOSE_BUTTONS; i++) {
            closeButtons[i].addEventListener("click", async () => {
                if (stickyHeader.style.visibility === "visible") {
                    if (stickyHeader.classList.contains("-translate-y-full")) {
                        stickyHeader.style.visibility = "hidden";
                    }
                    await transitionCloseView(dialog);
                    stickyHeader.style.visibility = "hidden";
                } else {
                    await transitionCloseView(dialog);
                }
            });
        }
    }

    const dialogContainer = dialog.querySelector(".dialog-container");

    dialogContainer.addEventListener("scroll", (event) => {
        if (event.target.scrollTop > header.offsetHeight) {
            stickyHeader.classList.remove("-translate-y-full");
        } else {
            stickyHeader.classList.add("-translate-y-full");
        }
    });
}

const views = document.querySelectorAll(".view-dialog");
views.forEach(async (dialog) => {
    await viewSetup(dialog);
});

const openViewsButtons = document.querySelectorAll("[data-view-id]");
openViewsButtons.forEach(async (button) => {
    const dialogs = document.querySelectorAll(".view-dialog");

    const id = button.getAttribute("data-view-id");
    const dialog = Array.from(dialogs).find((dialog) => dialog.id === id);
    await openViewButtonSetup(button, dialog);
});

/** MODALS */

const modalObj = {
    isDragging: false,
    startY: 0,
    startTransform: 0,
    currentTransform: 0,
    maxUpwardDrag: -50,
};

/**
 * @param {EventListenerOrEventListenerObject} event
 * @param {Element} dialog
 */
function handleDragStart(event, dialog) {
    if (!dialog.hasAttribute("open")) return;
    const touch = event.touches ? event.touches[0] : event;
    modalObj.isDragging = true;
    modalObj.startY = touch.clientY;
    const transform = new WebKitCSSMatrix(window.getComputedStyle(dialog).transform);
    modalObj.startTransform = transform.m42;
    dialog.style.transition = "none";
}

/**
 * @param {EventListenerOrEventListenerObject} event
 * @param {Element} dialog
 */
function handleDragMove(event, dialog) {
    if (!modalObj.isDragging) return;
    const touch = event.touches ? event.touches[0] : event;
    const deltaY = touch.clientY - modalObj.startY;

    /* Only allow closing on downward drag */
    if (deltaY < 0) {
        /* Upward drag with resistance */
        const resistance = 0.3;
        modalObj.currentTransform = Math.max(modalObj.maxUpwardDrag, deltaY * resistance + modalObj.startTransform);
    } else {
        /* Downward drag */
        modalObj.currentTransform = deltaY + modalObj.startTransform;
    }

    dialog.style.transform = `translateY(${modalObj.currentTransform}px)`;

    /* Only update backdrop opacity on downward drag */
    if (deltaY > 0) {
        const backdrop = dialog.previousElementSibling;
        if (backdrop && backdrop.matches("::modal-backdrop")) {
            const opacity = Math.max(0, 1 - modalObj.currentTransform / (window.innerHeight * 0.7));
            backdrop.style.backgroundColor = `rgba(0, 0, 0, ${opacity * 0.7})`;
        }
    }
}

/**
 * @param {Element} dialog
 */
function handleDragEnd(dialog) {
    if (!modalObj.isDragging) return;
    modalObj.isDragging = false;
    dialog.style.transition = "transform 0.3s cubic-bezier(0.4, 0, 0.2, 1)";

    const threshold = window.innerHeight * 0.1;
    /* Only close on downward drag */
    if (modalObj.currentTransform > threshold && modalObj.currentTransform > 0) {
        transitionCloseModal(dialog);
    } else {
        /* Reset position with a small bounce effect */
        dialog.style.transform = "translateY(0)";
        const backdrop = dialog.previousElementSibling;
        if (backdrop && backdrop.matches("::modal-backdrop")) {
            backdrop.style.backgroundColor = "rgba(0, 0, 0, 0.7)";
        }
    }
}

/**
 * @param {Element} dialog
 */
function resetModalPosition() {
    modalObj.isDragging = false;
    modalObj.startY = 0;
    modalObj.startTransform = 0;
    modalObj.currentTransform = 0;
}

/**
 * @param {Element} dialog
 */
async function modalSetup(dialog) {
    /* Touch events */
    dialog.addEventListener("touchstart", (event) => handleDragStart(event, dialog), {
        passive: true,
    });
    dialog.addEventListener("touchmove", (event) => handleDragMove(event, dialog), { passive: true });
    dialog.addEventListener("touchend", (_) => handleDragEnd(dialog));
    dialog.addEventListener("touchcancel", (_) => handleDragEnd(dialog));

    /* Mouse events */
    dialog.addEventListener("mousedown", (event) => handleDragStart(event, dialog));
    document.addEventListener("mousemove", (event) => handleDragMove(event, dialog));
    document.addEventListener("mouseup", (_) => handleDragEnd(dialog));

    dialog.addEventListener("click", async (event) => {
        if (event.target === dialog) {
            await transitionCloseModal(dialog);
        }
    });

    const closeButton = dialog.querySelector(".close-modal");
    closeButton.addEventListener("click", async () => await transitionCloseModal(dialog));

    dialog.addEventListener("close", () => {
        dialog.style.transform = "translateY(0)";
        resetModalPosition();
    });
}

/**
 * @param {Element} dialog
 */
async function transitionOpenModal(dialog) {
    const parentModal = dialog.parentNode.closest("dialog");
    if (parentModal) {
        parentModal.style.viewTransitionName = "none";
    }
    dialog.style.viewTransitionName = "modal-content";
    await startViewTransition(() => {
        dialog.showModal();
    });
}

/**
 * @param {Element} dialog
 */
async function transitionCloseModal(dialog) {
    await startViewTransition(() => {
        dialog.close();
    });
    dialog.style.viewTransitionName = "none";
    const parentModal = dialog.parentNode.closest("dialog");
    if (parentModal) {
        parentModal.style.viewTransitionName = "modal-content";
    }
}

/**
 * @param {Element} dialog
 */
async function openModal(dialog) {
    dialog.style.transform = "translateY(0)";

    resetModalPosition();
    await transitionOpenModal(dialog);
}

/**
 * @param {Element} button
 * @param {Element} dialog
 */
async function openModalButtonSetup(button, dialog) {
    button.addEventListener("click", async () => {
        await openModal(dialog);
    });
}

const modals = document.querySelectorAll(".modal-dialog");
modals.forEach(async (dialog) => {
    await modalSetup(dialog);
});

const openModalsButtons = document.querySelectorAll("[data-modal-id]");
openModalsButtons.forEach(async (button) => {
    const dialogs = document.querySelectorAll(".modal-dialog");

    const id = button.getAttribute("data-modal-id");
    const dialog = Array.from(dialogs).find((dialog) => dialog.id === id);
    await openModalButtonSetup(button, dialog);
});

/** INPUTS */

/**
 * @param {Element} element
 */
function textInputSetup(element) {
    const input = element.querySelector("input");
    const clearButton = element.querySelector(".clear-button");

    input.addEventListener("input", () => {
        clearButton.style.visibility = input.value ? "visible" : "hidden";
    });

    clearButton.addEventListener("click", () => {
        input.value = "";
        clearButton.style.visibility = "hidden";
    });
}

const textInputs = document.querySelectorAll(".text-input");
textInputs.forEach((container) => {
    textInputSetup(container);
});
