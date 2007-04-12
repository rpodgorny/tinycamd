
//
// Note: You will find that I like closures better than masses of global variables and suites
//       of functions to handle them. 
// 


function dump(obj,tag) {
    log('============='+tag+'===============');

    if ( !obj) log(" false ");
    else {
        switch( typeof(obj)) {
          case "object":
            log(obj.toString);
	    log(typeof(obj));
	    log(" .x = "+obj.x);
            for ( i in obj) log("  "+i+" = "+obj[i]);
            break;
          default:
            log(obj);
            break;
        }
    }
    
}

function log(message) {
    if (!log.window_ || log.window_.closed) {
        var win = window.open("", null, "width=400,height=300," +
                              "scrollbars=yes,resizable=yes,status=no," +
                              "location=no,menubar=no,toolbar=no");
        if (!win) return;
        var doc = win.document;
        doc.write("<html><head><title>Debug Log</title></head>" +
                  "<body></body></html>");
        doc.close();
        log.window_ = win;
    }
    var logLine = log.window_.document.createElement("div");
    logLine.appendChild(log.window_.document.createTextNode(message));
    log.window_.document.body.appendChild(logLine);
}

//
// Crappy utility to accomodate the beast of redmond
//
function createRequestObject() {
    var ro;
    var browser = navigator.appName;
    if(browser == "Microsoft Internet Explorer"){
        ro = new ActiveXObject("Microsoft.XMLHTTP");
    }else{
        ro = new XMLHttpRequest();
    }
    return ro;
}

//
// Just a wee utility function to help me out.
//
function FindPos(obj) {
    var curleft = curtop = 0;

    if (obj.offsetParent) {
	curleft += obj.offsetLeft;
	curtop += obj.offsetTop;
	while (obj = obj.offsetParent) {
	    curleft += obj.offsetLeft;
	    curtop += obj.offsetTop;
	}
    }
    return [curleft,curtop];
}

var preloads = new Array("frame-handle", "frame-handle-a",
			 "frame-t", "frame-tr", "frame-v", "frame-br", "frame-b", 
			 "slider", "slider-thumb");

for ( var i = 0; i < preloads.length; i++) {
    var n = new Image();
    n.src = preloads[i]+".png";
}

//
// This is the endless loop of frame loading. You could have as many of these going as you like,
// but one seems like the highest sane number, still I guess you could watch more than one camera and
// all would be well. (Uh huh, remember what I said about global variables vs. closures up above?)
//
function NextFrame(im,seq)
{
    seq++;
    im.onload = function() {
	setTimeout( function() { NextFrame(im,seq) }, 500);
    };

    im.src = "image.jpg?"+seq;
    //im.src = "tuna.jpg";
}

//
// Run through all the sliders and put on their thumbs.  We do this at the last minute because they 
// might move around and we end up using absolute positions to keep them from taking space.
//
function AddThumbs()
{
    var pattern = new RegExp("\\bslider\\b");
    var imgs = document.getElementsByTagName("img");

    for ( var i = 0; i < imgs.length; i++) {
	var e = imgs[i];
	if ( pattern.test(e.className) ) {
	    var t = document.createElement('img');
	    t.width = 11;
	    t.height = 11;
	    t.src = "slider-thumb.png";
	    var par = e.parentNode;
	    var pos = FindPos(e);
	    var ppos = FindPos(par);
	    
	    log("x="+e.x);
	    log("par="+par.tagName+"."+par.className);
	    log("pos=("+pos[0]+","+pos[1]+")  ("+e.offsetLeft+","+e.offsetTop+")");
	    log("ppos=("+ppos[0]+","+ppos[1]+")");
	    t.style.position = "absolute";
	    t.style.top = pos[1] - ppos[1] - (t.height-e.height)/2+"px";
	    t.style.left = pos[0] - ppos[0] - t.width/2 + (e.currentValue * e.width / (e.maximum - e.minimum)) +"px";
	    t.style.cursor = "pointer";
	    t.sliderBar = e;
	    t.onmousedown = ThumbDown;
	    par.appendChild(t);
	    log("pos=("+t.style.left+","+t.style.top+")  ("+t.offsetLeft+","+t.offsetTop+")");
	}
    } 
}

function BuildFrame()
{
    var d = document.getElementById('overlay');
    var pos = FindPos(d);
    var width = d.offsetWidth;
    var height = d.offsetHeight;
    var right = pos[0] + width;
    var bottom = pos[1] + height;

    // Strip the old frame
    var old = d.getElementsByTagName('img');
    for ( var i = old.length-1; i >= 0; i--) {
	if ( old[i].className == "frame") d.removeChild(old[i]);
    }
    var oldThumb = document.getElementById("controlhandle");
    if ( oldThumb) d.removeChild(oldThumb);

    var make = function( file, left,top, wid, hgt) {
	var t = document.createElement('img');
	t.width = wid;
	t.height = hgt;
	t.src = file;
	t.className = "frame";
	t.style.position = 'absolute';
	t.style.top = top+"px";
	t.style.left = left+"px";
	t.onmousedown = bail;
	d.appendChild(t);
    };

    make( 'frame-t.png', 0,0, width-9,9);
    make( 'frame-tr.png', width-9,0, 9,9);
    make( 'frame-b.png', 0,height-9, width-9,9);
    make( 'frame-br.png', width-9,height-9, 9,9);
    make( 'frame-v.png', width-9,9, 9, (height-2*9)/2 - 16/2);
    make( 'frame-v.png', width-9,9+(height-2*9)/2 + 16/2, 9,(height-2*9)/2 - 16/2);

    var thumb = document.createElement('a');
    //    thumb.href = "javascript:ToggleControls()";
    thumb.href = "javascript:ToggleControls()";
    thumb.id = "controlhandle";
    var ti = document.createElement('img');
    ti.width = 9;
    ti.height = 16;
    ti.src = "frame-handle.png";
    ti.className = "button";
    thumb.appendChild(ti);
    thumb.style.position = 'absolute';
    thumb.style.top = (height/2 - ti.height/2)+"px";
    thumb.style.left = (width - ti.width)+"px";
    d.appendChild(thumb);

    d.onmousedown = bail;
    d.hidden = 0;
    d.hide = function() {
	d.style.left = -(width - 9) + "px";
	d.hidden = 1;
    };
    d.unhide = function() {
	d.style.left = "0px";
	d.hidden = 0;
    };
       
}

function ThumbDown(e)
{
    if ( !e) e = window.event;
    
    var p = this.parentNode;
    var pos = FindPos(this);
    var s = this.sliderBar;
    var spos = FindPos(s);
    var ppos = FindPos(p);

    log("p="+p.tagName+"."+p.className);
    log("limit ("+pos[0]+","+pos[1]);
    var thumb = this;
    var leftLimit = spos[0];
    var rightLimit = leftLimit + s.width;
    var initClientX = e.clientX;
    var cdist = (this.width+1)/2;
    var deltaX = initClientX - pos[0] - cdist;
    var baseX = ppos[0];

    log("left="+leftLimit+" right="+rightLimit+" icx="+initClientX+" bx="+baseX+" dx="+deltaX);
    // arrange to restore the event handlers we are about to take
    var oldMouseMove = document.onmousemove;
    var oldMouseUp = document.onmouseup;
    document.onmouseup = function() {
	document.onmouseup = oldMouseUp;
	document.onmousemove = oldMouseMove;

	var http = createRequestObject();
	http.open('get', 'tinycamd/set?'+s.cid+'='+s.currentValue);
	log('tinycamd/set?'+s.cid+'='+s.currentValue);
	http.send(null);
    };
    
    // take some event handlers
    document.onmousemove = function(e) {
	if ( !e) e = window.event;
	
	// log("left="+leftLimit+" right="+rightLimit+" icx="+initClientX+" bx="+baseX+" ecx="+e.clientX);
	var x = e.clientX - deltaX;
    
	if ( x < leftLimit ) x = leftLimit;	
	if ( x > rightLimit) x = rightLimit;
    
	//    log("("+leftLimit+","+rightLimit+")"+initX+" n="+nx);

	var newValue = Math.round(s.minimum + (x-leftLimit)*s.maximum/(rightLimit-leftLimit));
	
	if ( newValue != s.currentValue) {
	    s.currentValue = newValue;
	    log("value: "+newValue);
	}
	var nx = x - baseX - cdist;
	//if ( nx == thumb.style.left ) return false;
	thumb.style.left = nx+"px";
	return false;
    };

    //    log("("+thumbingLeftLimit+","+thumbingRightLimit+")"+thumbingInitX);

    return false;
}

function ToggleControls()
{
    var d = document.getElementById('overlay');

    if ( !d) return;
    if ( d.hidden) d.unhide();
    else d.hide();

}

function bail(e)
{
    if ( !e) e = window.event;
    
    //log("bailing");
    
    e.stopPropagation();
    e.preventDefault();
    
    return false;
}


function LoadControls() {
    var http = createRequestObject();

    log("hello");
    http.open('get', 'tinycamd/controls');
    http.onreadystatechange = 
	function() {
	    if ( http.readyState != 4) return;
	    document.getElementById('controls').innerHTML = http.responseText;
	    
	    var cc = document.getElementById('cameracontrols');

	    while( cc.childNodes.length > 0) {
		cc.removeChild(cc.childNodes[0]);
	    }
	    
	    var ranges = http.responseXML.documentElement.getElementsByTagName("range_control");
	    for ( var j = 0; j < ranges.length; j++) {
		var r = ranges[j];
		log("Adding "+r.name);
		var d = document.createElement('div');
		d.className = 'rangecontrol';
		tn = document.createTextNode(r.name.toLowerCase()+" ");
		d.appendChild( tn);
		i = document.createElement('img');
		i.src = 'slider-l.png';
		i.width = 6;
		i.height = 9;
		d.appendChild(i);
		i = document.createElement('img');
		i.className = 'slider';
		i.src = 'slider.png';
		i.width = 200;
		i.height = 9;
		i.minimum = parseInt(r.minimum);
		i.maximum = parseInt(r.maximum);
		i.currentValue = parseInt(r.current);
		if ( i.currentValue > i.maximum) i.maximum = i.currentValue;
		//i.defaultValue = parseInt(r.default);
		i.cid = parseInt(r.cid);
		d.appendChild(i);
		i = document.createElement('img');
		i.src = 'slider-r.png';
		i.width = 6;
		i.height = 9;
		d.appendChild(i);
		cc.appendChild( d);
	    }
	    BuildFrame();
	    AddThumbs();
	    //log( http.getAllResponseHeaders());
	    //dump(http.responseXML.documentElement, http.statusText);
	};
    http.send(null);
}

function Initialize()
{
    AddThumbs();
    BuildFrame();
    LoadControls();
    //NextFrame( document.getElementById('cam'), 1);
}


