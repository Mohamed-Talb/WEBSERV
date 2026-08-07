const log = document.querySelector("#log");
const createBtn = document.querySelector("#createBtn");
const readBtn = document.querySelector("#readBtn");
const updateBtn = document.querySelector("#updateBtn");
const listBtn = document.querySelector("#listBtn");
const destroyBtn = document.querySelector("#destroyBtn");
const clearLogBtn = document.querySelector("#clearLog");

function getSessionPath() {
  return document.querySelector("#sessionPath").value.trim()
    || "/cgi_bin/session.py";
}

function getUsername() {
  return document.querySelector("#sessionUsername").value.trim()
    || "guest";
}

function getSessionValue() {
  return document.querySelector("#sessionValue").value.trim()
    || "webserv";
}

async function sessionRequest(action, method, includeValues) {
  const parameters = new URLSearchParams();

  parameters.set("action", action);

  if (includeValues) {
    parameters.set("username", getUsername());
    parameters.set("value", getSessionValue());
  }

  const path = getSessionPath() + "?" + parameters.toString();
  const url = wsUrl(path);

  const options = {
    method: method,
    credentials: "same-origin"
  };

  log.textContent = "";

  wsLog(log, "Request:", "type");
  wsLog(log, `${method} ${url}`, "k");

  wsAddEmptyLines(log, 2);
  wsLog(log, "Response:", "type");

  try {
    const response = await fetch(url, options);
    const text = await response.text();

    wsLog(
      log,
      `${response.status} ${response.statusText}`,
      response.ok ? "ok" : "err"
    );

    response.headers.forEach((value, name) => {
      wsLog(log, `${name.toUpperCase()}: ${value}`);
    });

    wsLog(log, "");
    wsLog(log, "Body:", "type");
    wsLog(log, text || "(empty)");
  }
  catch (error) {
    wsLog(log, "REQUEST FAILED: " + error.message, "err");
  }
}

createBtn.addEventListener("click", () => {
  sessionRequest("create", "POST", true);
});

readBtn.addEventListener("click", () => {
  sessionRequest("read", "GET", false);
});

updateBtn.addEventListener("click", () => {
  sessionRequest("update", "POST", true);
});

listBtn.addEventListener("click", () => {
  sessionRequest("list", "GET", false);
});

destroyBtn.addEventListener("click", () => {
  sessionRequest("destroy", "POST", false);
});

clearLogBtn.addEventListener("click", () => {
  log.textContent = "";
});