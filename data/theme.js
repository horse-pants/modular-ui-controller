function toggleTheme() {
    const body = document.body;
    const toggle = document.querySelector('.theme-toggle');

    body.classList.toggle('light');

    if (body.classList.contains('light')) {
        if (toggle) toggle.innerHTML = '<i class="fas fa-moon"></i>';
        localStorage.setItem('theme', 'light');
    } else {
        if (toggle) toggle.innerHTML = '<i class="fas fa-sun"></i>';
        localStorage.setItem('theme', 'dark');
    }
}

document.addEventListener('DOMContentLoaded', function() {
    const savedTheme = localStorage.getItem('theme');
    const toggle = document.querySelector('.theme-toggle');

    if (savedTheme === 'light') {
        document.body.classList.add('light');
        if (toggle) toggle.innerHTML = '<i class="fas fa-moon"></i>';
    } else {
        if (toggle) toggle.innerHTML = '<i class="fas fa-sun"></i>';
    }
});