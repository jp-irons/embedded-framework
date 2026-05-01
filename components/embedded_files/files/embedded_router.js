//
// embedded/router.js
//
// Minimal hash-based SPA router.
// Usage:
//   import { initRouter } from "/embedded/router.js";
//   initRouter({ routes, fallback: "#home" });
//
// Each route is: { hash: "#wifi", mount: async (container) => { ... } }
// mount() receives the #app container and is responsible for populating it.
//

export function initRouter({ routes, fallback = "#home" }) {
    const app = document.getElementById("app");

    async function navigate() {
        const hash = location.hash || fallback;
        const route = routes.find(r => r.hash === hash) 
                   ?? routes.find(r => r.hash === fallback);

        if (!route) {
            app.innerHTML = `<p class="text-red-600">404 — no route for ${hash}</p>`;
            return;
        }

        // Clear previous view
        app.innerHTML = "";

        // Let the route populate the container
        await route.mount(app);
    }

    window.addEventListener("hashchange", navigate);

    // Handle initial load (page load or hard refresh)
    navigate();
}
