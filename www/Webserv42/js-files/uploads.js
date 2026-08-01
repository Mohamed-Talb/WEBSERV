const listEl = document.querySelector("#fileList");
const log = document.querySelector("#log");
const refreshBtn = document.querySelector("#refreshBtn");
const clearBtn = document.querySelector("#clearLog");

clearBtn.addEventListener("click", () => {
  log.textContent = "";
});

function renderEmpty(message) {
  listEl.innerHTML = `<li><span class="fname">${wsEscape(message.toUpperCase())}</span></li>`;
}

function renderFromNames(names) {
  if (!names.length) {
    renderEmpty("DIRECTORY IS EMPTY");
    return;
  }

  const listPath = document.querySelector("#listPath").value.replace(/\/+$/, "");

  function fileUrl(name) {
    if (String(name).startsWith("/"))
      return wsUrl(String(name));

    return wsUrl(listPath + "/" + name);
  }

  listEl.innerHTML = names.map((name, index) => {
    const url = wsEscape(fileUrl(name));

    return `
      <li>
        <span class="fname">${wsEscape(name)}</span>
        <span>
          <a class="fmeta" href="${url}" target="_blank">OPEN ↗</a>
          <button class="file-delete" data-index="${index}">DELETE</button>
        </span>
      </li>
    `;
  }).join("");

  listEl.querySelectorAll(".file-delete").forEach((button) => {
    button.addEventListener("click", async () => {
      const name = names[Number(button.dataset.index)];

      if (!confirm(`DELETE ${name}?`))
        return;

      const url = fileUrl(name);
      const response = await fetch(url, { method: "DELETE" });

      wsLog(log, `DELETE ${url} → ${response.status} ${response.statusText}`, response.ok ? "ok" : "err");

      if (response.ok)
        refreshBtn.click();
    });
  });
}

function parseHtmlIndex(html) {
  const documentIndex = new DOMParser().parseFromString(html, "text/html");

  return Array.from(documentIndex.querySelectorAll("a"))
    .map((link) => link.getAttribute("href"))
    .filter((href) => href && href !== "../" && href !== "/")
    .map((href) => decodeURIComponent(href.replace(/\/$/, "")));
}

refreshBtn.addEventListener("click", async () => {
  const path = document.querySelector("#listPath").value || "/upload/";
  const url = wsUrl(path);

  log.style.display = "block";
  log.textContent = "";

  wsLog(log, "Request:", "type");
  wsLog(log, `GET ${url}`, "k");
  wsAddEmptyLines(log, 2);
  wsLog(log, "Response:", "type");

  refreshBtn.disabled = true;
  refreshBtn.textContent = "LOADING…";

  try {
    const response = await fetch(url, {
      cache: "no-store"
    });

    const text = await response.text();

    wsLog(log, `${response.status} ${response.statusText}`, response.ok ? "ok" : "err");

    if (!response.ok) {
      renderEmpty(`SERVER RETURNED ${response.status}`);
      return;
    }

    renderFromNames(parseHtmlIndex(text));
  }
  catch (error) {
    wsLog(log, "REQUEST FAILED: " + error.message, "err");
    renderEmpty("COULD NOT REACH SERVER");
  }
  finally {
    refreshBtn.disabled = false;
    refreshBtn.textContent = "REFRESH LIST";
  }
});