var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

$(document).ready(function(){
    initWebSocket();
});

function setAnimation(animation){
    var text = $("#animationItems").find("[data-value="+ animation +"]").text();
    $("#animationDropDown").find('.dropdown-toggle').text(" "+text+" ");    
}

function snuh(clickEvent){
    if (clickEvent?.target) {        
        selectValue = parseInt($(clickEvent.target).data('value'), 10)
        if(selectValue > -1){
            $("#animationDropDown").find('.dropdown-toggle').text( " " + $(clickEvent.target).text() + " ");
            $("#animation").addClass("active");

            websocket.send(JSON.stringify(
                {
                    "message": "animation", 
                    "value": true,
                    "animation": $(clickEvent.target).data('value') 
                }));
        
        }
        else{
            $("#animation").text("Animations");
            $("#animation").removeClass("active");
            websocket.send(JSON.stringify(
                {
                    "message": "animation", 
                    "value": false
                }));
        }      
    }
}

function changeBrightness(element){
    websocket.send(JSON.stringify(
        {
            "message" : element.id, 
            "value": parseInt($("#"+element.id).val())
        }));
}

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    websocket.send(JSON.stringify(
        {
            "message" : "connect"
        }));    
}
  
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
} 

function onMessage(event) {
    var myObj = JSON.parse(event.data);
            console.log(myObj);
            if(myObj.message === "states"){
            for (i in myObj.controls){

                var name = myObj.controls[i].name;
                var state = myObj.controls[i].state;

                if (state == true){
                    $("#" + name).addClass("active");
                    if (name == "animation"){
                        var animation = myObj.controls[i].animation;
                        setAnimation(animation);
                    }
                }
                else if (state == false){
                    $("#" + name).removeClass("active");
                }
                else if (name=="colour"){
                    document.getElementById(name).value = state;
                }

            }
        }
        else{
            $("#animationItems")
            .empty()
            .append(myObj.animations.map(d => `<li><a data-value=${d.value}>${d.name}</a></li>`));

            $('#animationDropDown').on('hide.bs.dropdown', ({ clickEvent }) => snuh(clickEvent));

        }
    console.log(event.data);
}

function toggleControl (element) {
    console.log(element.id);

    websocket.send(JSON.stringify(
        {
            "message" : element.id, 
            "value": $("#"+element.id).hasClass('active') 
        }));
}

function colourChange (element) {
    websocket.send(JSON.stringify(
        {
            "message" : element.id, 
            "value": element.value
        }));    
}


