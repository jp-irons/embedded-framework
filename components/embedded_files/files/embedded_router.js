//
// embedded/router.js
//
// Minimal hash-based SPA router with teardown support.
// Usage:
//   import { initRouter } from "/embedded/router.js";
//   initRouter({ routes, fallback: "#home" });
//
// Each route:
//   {
//     hash:     "#wifi",
//     mount:    async (container) => { ... },  // required
//     teardown: () => { ... }                  // optional, called before next route mounts
//   }
//

let currentRoute = null;

export function initRouter({ routes, fallback = "#home" }) {
    const app = document.getElementById("app");

    async function navigate() {
        const hash = location.hash || fallback;
        const route = routes.find(r => r.hash === hash)
                   ?? routes.find(r => r.hash === fallback);

        // Teardown the current route before leaving it
        if (currentRoute?.teardown) {
            currentRoute.teardown();
        }

        currentRoute = null;
        app.innerHTML = "";

        if (!route) {
            app.innerHTML = `<p class="text-red-600">404 — no route for ${hash}</p>`;
            return;
        }

        await route.mount(app);
        currentRoute = route;
    }

    window.addEventListener("hashchange", navigate);
    navigate();
}
