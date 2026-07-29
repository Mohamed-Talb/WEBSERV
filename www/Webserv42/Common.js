function wsUrl(path) {
  return path.startsWith("/") ? path : "/" + path;
}

function wsEscape(value) {
  return String(value)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}

function wsLog(element, message, className) {
  const line = document.createElement("div");
  line.textContent = message;

  if (className) {
    line.className = className;
  }

  element.appendChild(line);
  element.scrollTop = element.scrollHeight;
}

document.addEventListener("DOMContentLoaded", function () {
  const currentPage =
    window.location.pathname.split("/").pop() || "index.html";

  const statusDot = document.querySelector("#statusDot");

  document.querySelectorAll(".nav a").forEach(function (link) {
    if (link.getAttribute("href") === currentPage) {
      link.classList.add("active");
    }
  });
});