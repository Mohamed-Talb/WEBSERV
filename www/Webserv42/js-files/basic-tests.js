const log = document.querySelector("#log");
const diagnosticButtons = document.querySelectorAll("button[data-path]");

diagnosticButtons.forEach((button) => {
  button.addEventListener("click", async () => {
    log.textContent = "";

    const path = button.dataset.path;
    const method = button.dataset.method || "GET";
    const expectedStatus = Number(button.dataset.expected);
    const isRedirectTest = button.dataset.redirectTest === "true";
    const isBinaryResponse = button.dataset.binary === "true";

    const options = {
      method: method,
      cache: "no-store",
      credentials: "include"
    };

    if (button.dataset.body && method !== "GET" && method !== "HEAD")
      options.body = button.dataset.body;

    if (button.dataset.contentType) {
      options.headers = {
        "Content-Type": button.dataset.contentType
      };
    }

    const url = wsUrl(path);

    wsLog(log, "Request:", "type");
    wsLog(log, `${method} ${url}`, "k");
    wsAddEmptyLines(log, 2);
    wsLog(log, "Response:", "type");

    try {
      const response = await fetch(url, options);

      let passed = false;
      let expectedMessage = "";

      if (isRedirectTest) {
        const finalUrl = new URL(response.url);
        const expectedFinalPath = button.dataset.finalPath;

        passed = response.redirected && finalUrl.pathname === expectedFinalPath;
        expectedMessage = `EXPECTED REDIRECTION TO: ${expectedFinalPath}`;
      }
      else {
        passed = !expectedStatus || response.status === expectedStatus;
        expectedMessage = `EXPECTED STATUS: ${expectedStatus}`;
      }

      wsLog(log, `${response.status} ${response.statusText} — ${passed ? "PASS" : "FAIL"}`, passed ? "ok" : "err");

      if (!passed)
        wsLog(log, expectedMessage, "err");

      if (isRedirectTest) {
        wsLog(log, `REDIRECTED: ${response.redirected}`);
        wsLog(log, `FINAL URL: ${response.url}`);
      }

      response.headers.forEach((value, name) => {
        wsLog(log, `${name.toUpperCase()}: ${value}`);
      });

      if (method !== "HEAD" && !isBinaryResponse) {
        const text = await response.text();

        if (text)
          wsLog(log, "\n" + text.slice(0, 500));
      }
    }
    catch (error) {
      wsLog(log, "REQUEST FAILED: " + error.message, "err");
    }
  });
});