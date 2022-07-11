const button = document.querySelector(".newBtn");
let clickCount = 0;

button.addEventListener("click", () => {
    clickCount += 1;
    button.textContent = `Counter: ${clickCount}`;
})