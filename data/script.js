const gateway = `ws://${window.location.hostname}/ws`;
let websocket;

document.addEventListener('DOMContentLoaded', function() {
    initWebSocket();
    initColorPicker();
});

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    websocket.send(JSON.stringify({ "message": "connect" }));
}

function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    const data = JSON.parse(event.data);
    console.log(data);
    
    if (data.message === "states") {
        data.controls.forEach(control => {
            const element = document.getElementById(control.name);
            if (!element) return;
            
            if (control.state === true) {
                element.classList.add("active");
                if (control.name === "animation" && control.animation !== undefined) {
                    setAnimationSelect(control.animation);
                }
            } else if (control.state === false) {
                element.classList.remove("active");
            } else if (control.name === "colour") {
                updateColorDisplay(control.state);
            } else if (control.name === "brightness") {
                element.value = control.state;
            }
        });
    } else if (data.animations) {
        populateAnimations(data.animations);
    }
}

function populateAnimations(animations) {
    const select = document.getElementById('animation');
    select.innerHTML = '<option value="-1">Select Animation</option>';
    
    animations.forEach(anim => {
        const option = document.createElement('option');
        option.value = anim.value;
        option.textContent = anim.name;
        select.appendChild(option);
    });
}

function setAnimationSelect(animationValue) {
    const select = document.getElementById('animation');
    select.value = animationValue;
    select.classList.add("active");
}

function animationChange(element) {
    const value = parseInt(element.value);
    
    if (value > -1) {
        element.classList.add("active");
        websocket.send(JSON.stringify({
            "message": "animation",
            "value": true,
            "animation": value
        }));
    } else {
        element.classList.remove("active");
        websocket.send(JSON.stringify({
            "message": "animation",
            "value": false
        }));
    }
}

function toggleControl(element) {
    const isActive = element.classList.contains('active');
    websocket.send(JSON.stringify({
        "message": element.id,
        "value": !isActive
    }));
}

function changeBrightness(element) {
    websocket.send(JSON.stringify({
        "message": element.id,
        "value": parseInt(element.value)
    }));
}

let currentColor = '#ff0000';
let colorPickerOpen = false;

function initColorPicker() {
    const colorDisplay = document.getElementById('colour');
    const colorGradient = document.getElementById('colorGradient');
    const presetColors = document.querySelectorAll('.preset-color');
    const hexInput = document.getElementById('hexInput');
    
    colorDisplay.style.backgroundColor = currentColor;
    hexInput.value = currentColor;
    
    colorGradient.addEventListener('click', function(e) {
        // Make sure we're getting the gradient element, not the ::after overlay
        const target = e.currentTarget; // Use currentTarget instead of target
        const rect = target.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const percent = x / rect.width;
        
        // Map to the exact colors and percentages in the CSS gradient
        const gradientStops = [
            { percent: 0.0000, color: '#ff0000' },    // 0% - Red
            { percent: 0.1666, color: '#ff8000' },    // 16.66% - Orange  
            { percent: 0.3333, color: '#ffff00' },    // 33.33% - Yellow
            { percent: 0.5000, color: '#00ff00' },    // 50% - Green
            { percent: 0.6666, color: '#00ffff' },    // 66.66% - Cyan
            { percent: 0.8333, color: '#0000ff' },    // 83.33% - Blue
            { percent: 1.0000, color: '#ff00ff' }     // 100% - Magenta
        ];
        
        // Find the two stops to interpolate between
        let color;
        if (percent <= 0) {
            color = gradientStops[0].color;
        } else if (percent >= 1) {
            color = gradientStops[gradientStops.length - 1].color;
        } else {
            // Find the segment
            let lowerStop = gradientStops[0];
            let upperStop = gradientStops[1];
            
            for (let i = 0; i < gradientStops.length - 1; i++) {
                if (percent >= gradientStops[i].percent && percent <= gradientStops[i + 1].percent) {
                    lowerStop = gradientStops[i];
                    upperStop = gradientStops[i + 1];
                    break;
                }
            }
            
            // Calculate interpolation factor
            const segmentPercent = (percent - lowerStop.percent) / (upperStop.percent - lowerStop.percent);
            color = interpolateColor(lowerStop.color, upperStop.color, segmentPercent);
        }
        
        
        setColor(color);
    });
    
    presetColors.forEach(preset => {
        preset.addEventListener('click', function() {
            setColor(this.dataset.color);
        });
    });
    
    hexInput.addEventListener('input', function() {
        const value = this.value;
        if (isValidHex(value)) {
            setColor(value);
        }
    });
    
    document.addEventListener('click', function(e) {
        if (!e.target.closest('.color-picker-container')) {
            hideColorPicker();
        }
    });
}

function toggleColorPicker() {
    const picker = document.getElementById('colorPicker');
    const colorControl = document.querySelector('.color-picker-container').closest('.control');
    
    if (colorPickerOpen) {
        hideColorPicker();
    } else {
        picker.style.display = 'block';
        colorControl.classList.add('color-picker-active');
        colorPickerOpen = true;
    }
}

function hideColorPicker() {
    const picker = document.getElementById('colorPicker');
    const colorControl = document.querySelector('.color-picker-container').closest('.control');
    
    picker.style.display = 'none';
    colorControl.classList.remove('color-picker-active');
    colorPickerOpen = false;
}

function updateColorDisplay(color) {
    currentColor = color;
    const colorDisplay = document.getElementById('colour');
    const hexInput = document.getElementById('hexInput');
    
    colorDisplay.style.backgroundColor = color;
    hexInput.value = color;
}

function setColor(color) {
    updateColorDisplay(color);
    websocket.send(JSON.stringify({
        "message": "colour",
        "value": color
    }));
}

function interpolateColor(color1, color2, factor) {
    // Convert hex to RGB
    const hex1 = color1.replace('#', '');
    const hex2 = color2.replace('#', '');
    
    const r1 = parseInt(hex1.substr(0, 2), 16);
    const g1 = parseInt(hex1.substr(2, 2), 16);
    const b1 = parseInt(hex1.substr(4, 2), 16);
    
    const r2 = parseInt(hex2.substr(0, 2), 16);
    const g2 = parseInt(hex2.substr(2, 2), 16);
    const b2 = parseInt(hex2.substr(4, 2), 16);
    
    // Interpolate
    const r = Math.round(r1 + (r2 - r1) * factor);
    const g = Math.round(g1 + (g2 - g1) * factor);
    const b = Math.round(b1 + (b2 - b1) * factor);
    
    // Convert back to hex
    return '#' + 
        r.toString(16).padStart(2, '0') + 
        g.toString(16).padStart(2, '0') + 
        b.toString(16).padStart(2, '0');
}

function hslToHex(h, s, l) {
    l /= 100;
    const a = s * Math.min(l, 1 - l) / 100;
    const f = n => {
        const k = (n + h / 30) % 12;
        const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
        return Math.round(255 * color).toString(16).padStart(2, '0');
    };
    return `#${f(0)}${f(8)}${f(4)}`;
}

function isValidHex(hex) {
    return /^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$/.test(hex);
}

function colourChange(element) {
    websocket.send(JSON.stringify({
        "message": element.id,
        "value": element.value
    }));
}


