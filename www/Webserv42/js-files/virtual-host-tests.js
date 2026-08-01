const copyLog = document.querySelector("#copyLog");
const copyButtons = document.querySelectorAll(".copy-command");

copyButtons.forEach((button) => {
  button.addEventListener("click", async () => {
    const target = document.querySelector("#" + button.dataset.target);
    const command = target.textContent.trim();

    try {
      await navigator.clipboard.writeText(command);
      copyLog.textContent = "";
      wsLog(copyLog, "COMMAND COPIED", "ok");
    }
    catch (error) {
      copyLog.textContent = "";
      wsLog(copyLog, "COPY FAILED: SELECT THE COMMAND MANUALLY", "err");
    }
  });
});