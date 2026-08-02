const log = document.querySelector("#log");
const clearBtn = document.querySelector("#clearBtn");
const runBtn = document.querySelector("#runBtn");

clearBtn.addEventListener("click", () => {
  log.textContent = "";
});

runBtn.addEventListener("click", async () => {
  log.textContent = "";

  const path = document.querySelector("#cgiPath").value || "/cgi_bin/test.py";
  const method = document.querySelector("#cgiMethod").value;
  const query = document.querySelector("#cgiQuery").value.trim();
  const body = document.querySelector("#cgiBody").value;
  const contentType = document.querySelector("#cgiContentType").value;

  let fullPath = path;

  if (query)
    fullPath += "?" + query;

  const url = wsUrl(fullPath);
  const options = { method: method };

  if (method === "POST") {
    options.headers = {
      "Content-Type": contentType
    };

    options.body = body;
  }

  wsLog(log, "Request:", "type");
  wsLog(log, `${method} ${url}`, "k");

  if (method === "POST") {
    wsLog(log, `CONTENT-TYPE: ${contentType}`);
    wsLog(log, `CONTENT-LENGTH: ${new TextEncoder().encode(body).length}`);

    wsLog(log, "");
    wsLog(log, "Body:", "type");
    wsLog(log, body || "(empty)");
  }

  wsAddEmptyLines(log, 2);
  wsLog(log, "Response:", "type");

  try {
    const response = await fetch(url, options);
    const text = await response.text();

    wsLog(log, `${response.status} ${response.statusText}`, response.ok ? "ok" : "err");

    response.headers.forEach((value, name) => {
      wsLog(log, `${name.toUpperCase()}: ${value}`);
    });

    if (text) {
      wsLog(log, "");
      wsLog(log, "Body:", "type");
      wsLog(log, text);
    }
  }
  catch (error) {
    wsLog(log, "REQUEST FAILED: " + error.message, "err");
  }
});