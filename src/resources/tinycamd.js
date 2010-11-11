
var baseUrl = '';

function loadControls( data,status) {
    var pendingChanges = {};
    var currentRequest;
    
    function doneChange(xhr, status) {
	$('#log').text( currentRequest.url + ' '+ status + '\n' + $('#log').text());
	currentRequest = false;
	doChange();
    };
    
    function doChange() {
	if ( currentRequest) return;       // don't ever let more than one go.
	
	for ( var cid in pendingChanges) {
	    currentRequest = { 'url': baseUrl+'set?'+cid+'='+pendingChanges[cid],
			       dataType: 'text',
			       complete: doneChange };
	    delete pendingChanges[cid];
	    $.ajax( currentRequest);
	    break;  // only do the first one
	}
    };
    function noteChange(cid,val) {
	pendingChanges[''+cid] = val;
	doChange();
    };

    function toggleRefresh() {
	if ( $(this).hasClass('on') ) {
	    $(this).removeClass('on').addClass('off').attr('value','stopped');
	} else {
	    $(this).removeClass('off').addClass('on').attr('value','running');
	    startRefresh();
	}
    }

    $('TABLE#controls').append( $('<tr>')
				.append( $('<th>').text('Refresh'))
				.append( $('<td>')
					 .append( $('<input type="button" class="off" id="refresh" value="stopped">').click(toggleRefresh))));
    function doControl( ) {
	var c = $('<tr>').append( $('<th>').text( this.getAttribute('name')));

	var cid = this.getAttribute('cid');
	switch( this.tagName) {
	  case 'range_control':
	    c.append( $('<td>').append( $('<input type=range>')
					.attr('min', this.getAttribute('minimum'))
					.attr('max', this.getAttribute('maximum'))
					.attr('value', this.getAttribute('current'))
					.attr('step', this.getAttribute('by'))
					.attr('data-default', this.getAttribute('default'))
					.change( function() { noteChange( cid,this.value); })));
	    break;
	  case 'boolean_control':
	    c.append( $('<td>').append( $('<input type=checkbox>')
					.attr('checked', this.getAttribute('current') != 0 ? true : false)
					.attr('data-default', this.getAttribute('default'))
					.change( function() { noteChange( cid,this.checked ? 1 : 0); })));
	    break;
	  case 'menu_control':
	    var g = $('<td>');
	    var radioname = this.getAttribute('name');
	    var current = this.getAttribute('current');
	    $(this).find('menu_item').each( function() {
		    var index = this.getAttribute('index');
		    var r = $('<input type=radio>')
			.attr('name', radioname)
			.attr('value', this.getAttribute('index'))
			.attr('checked', this.getAttribute('index') == current )
			.attr('data-default', this.getAttribute('default'))
			.change( function() { noteChange( cid, index); });
		    g.append( $('<div>')
			      .append(r)
			      .append( $('<span>').text( this.getAttribute('name'))));
		});
	    c.append(g);
	    break;
	}

	if ( c) $('TABLE#controls').append( c);
    };

    $(data).find('controls').children().each( doControl);
}

var refreshRunning = false;

function startRefresh()
{
    if ( refreshRunning) return;

    var serial = (new Date).getTime();
    var nth = 0;
    var theImage = $('<img>').load(gotImage);

    function loadNext() {
	$(theImage).attr('src', baseUrl+'image.jpg'+'?'+serial+'.'+nth++);
    }
    
    function gotImage() {
	$('IMG#picture')[0].src = $(theImage)[0].src;
	
	if ( $('INPUT#refresh').hasClass('on') ) setTimeout( loadNext, 100);
	else refreshRunning = false;
    };
    loadNext();
}

$(document).ready( function() {
	// load the controls
	$.ajax( {url: baseUrl+'controls',
		    dataType: 'xml',
		    success: function ( data,status) { $(document).ready( function() { loadControls( data,status); }) } });
    });
