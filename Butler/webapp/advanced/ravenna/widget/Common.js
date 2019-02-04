///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function AddEvent(szElementID, szEvent, fnCallBack) 
{
	try
	{
		var eElement = document.getElementById(szElementID);
		if (eElement)
		{
/*
			try 
			{
				eElement.attachEvent("on" + szEvent, fnCallBack); //For IE
			}
			catch(e) 
*/
			{
				try 
				{
					eElement.addEventListener(szEvent, fnCallBack, false); //For other browsers
				}
				catch(e) 
				{
				}
			}
		}
	}
	catch(e) 
	{
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function DetachEvent(szElementID, szEvent, fnCallBack) {
    try {
        var eElement = document.getElementById(szElementID);
        if (eElement)
        {
/*
            try
            {
                eElement.detachEvent("on" + szEvent, fnCallBack); //For IE
            }
            catch (e)
*/
            {
                try
                {
                    eElement.removeEventListener(szEvent, fnCallBack, false); //For other browsers
                }
                catch (e)
                {
                }
            }
        }
    }
    catch (e)
    {
    }
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function SendAction(szAction)
{
	var reqSendAction = null;
	if (window.XMLHttpRequest)
	{
		reqSendAction = new XMLHttpRequest();
	}
	else
	{
		reqSendAction = new ActiveXObject("Msxml2.XMLHTTP");
		
	}
	if (reqSendAction == null)
	{
		//alert("Your browser doesn't support AJAX.");
		return false;
	}

	reqSendAction.open('GET', szAction + '&ClientID=' + g_nClientID + AJAXCacheBreaker(), true);
	reqSendAction.send(null);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function SyncSendAction(szAction)
{
	var reqSendAction = null;
	if (window.XMLHttpRequest)
	{
		reqSendAction = new XMLHttpRequest();
	}
	else
	{
		reqSendAction = new ActiveXObject("Msxml2.XMLHTTP");
		
	}
	if (reqSendAction == null)
	{
		//alert("Your browser doesn't support AJAX.");
		return false;
	}

	reqSendAction.open('GET', szAction + '&ClientID=' + g_nClientID + AJAXCacheBreaker(), false);
	reqSendAction.send(null);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function PostActionData(szAction, szData)
{
	var reqPostAction = null;
	if (window.XMLHttpRequest)
	{
		reqPostAction = new XMLHttpRequest();
	}
	else
	{
		reqPostAction = new ActiveXObject("Msxml2.XMLHTTP");
		
	}
	if (reqPostAction == null)
	{
		//alert("Your browser doesn't support AJAX.");
		return false;
	}

	reqPostAction.open('POST', szAction + '&ClientID=' + g_nClientID + AJAXCacheBreaker(), true);
	reqPostAction.send(szData);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function SendRequest(szRequest, fnRequestHandler)
{
	var reqSendRequest;

	if (typeof window.ActiveXObject !== 'undefined')
	{
		reqSendRequest = new ActiveXObject("Msxml2.XMLHTTP");
		reqSendRequest.onreadystatechange = fnRequestHandler;
	}
	else
	{
		reqSendRequest = new XMLHttpRequest();
		reqSendRequest.onload = fnRequestHandler;
	}
	if (reqSendRequest == null)
	{
		//alert("Your browser doesn't support AJAX.");
		return false;
	}

	try
	{
		reqSendRequest.open('GET', szRequest + '&ClientID=' + g_nClientID + AJAXCacheBreaker(), true);
	}
	catch (e)
	{
		//alert(e);
	}

	reqSendRequest.send("");
	
	return reqSendRequest;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function OpenTag(szTag, szID, szSRC, nX, nY, nWidth, nHeight, nZ, szStyles, szClass, szExtra)
{
	var szHTML;
	
	szHTML = "<" + szTag + " id='" + szID + "' \n";
		if (szSRC != null)
		{
			szHTML += "src='" + szSRC + "' \n";
		}
		
		if (szStyles != null)
		{
			szHTML += "style='\n";
				if (nX != null)
				{
					szHTML += "left:" + nX + "px;\n";
				}
				if (nY != null)
				{
					szHTML += "top:" + nY + "px;\n";
				}
				if (nWidth != null)
				{
					szHTML += "width:" + nWidth + "px;\n";
				}
				if (nHeight != null)
				{
					szHTML += "height:" + nHeight + "px;\n";
				}
				if (nZ != null)
				{
					szHTML += "z-index:" + nZ + ";\n";
				}
				if (szStyles != null)
				{
					szHTML += szStyles + "\n";
				}
			szHTML += "' \n";
		}
		
		if (szClass != null)
		{
			szHTML += "class='\n";
				szHTML += szClass;
			szHTML += "' \n";
		}

		if (szExtra != null)
		{
			szHTML += szExtra;
		}

	szHTML += ">\n";
	
	return szHTML;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function CloseTag(szTag)
{
	return "</" + szTag + ">\n";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function OpenCanvas(szID, nX, nY, nWidth, nHeight, nZ, szStyles, szClass, szExtra) {
    var szHTML;

    szHTML = "<canvas id='" + szID + "' ";

    if (nWidth != null) {
        szHTML += "width='" + nWidth + "' ";
    }
    if (nHeight != null) {
        szHTML += "height='" + nHeight + "' ";
    }
   
    if (szStyles != null) {
        szHTML += "style='";
        if (nX != null) {
            szHTML += "left:" + nX + "px; ";
        }
        if (nY != null) {
            szHTML += "top:" + nY + "px; ";
        }
       
        if (nZ != null) {
            szHTML += "z-index:" + nZ + "; ";
        }
        if (szStyles != null) {
            szHTML += szStyles + " ";
        }
        szHTML += "'\n";
    }

    if (szClass != null) {
        szHTML += "class='";
        szHTML += szClass;
        szHTML += "' \n";
    }

    if (szExtra != null) {
        szHTML += szExtra;
    }

    szHTML += ">\n";

    return szHTML;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function CloseCanvas() {
    return "</canvas>\n";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function SetElementText(elem, changeVal) {
    if(elem) {
        if ((elem.textContent) && (typeof (elem.textContent) != "undefined")) {
            elem.textContent = changeVal;
        } else {
            elem.innerText = changeVal;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function AddButton(szID, szSRC, nX, nY, nZ, szExtra)
{
	//return OpenTag("img", szID, szSRC, nX, nY, null, null, nZ, "position:absolute;", null, szExtra) + CloseTag("img");

	var nOffset = c_oButtonSize.width * (1 - (1 / g_nButtonReductionFactor)) / 2;
	return OpenTag("img", szID, szSRC, nX + nOffset, nY + nOffset, c_oButtonSize.width / g_nButtonReductionFactor, c_oButtonSize.height / g_nButtonReductionFactor, nZ, "position:absolute;", null, szExtra) + CloseTag("img");
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function AddImage(szID, szSRC, nX, nY, nWidth, nHeight, nZ, szExtra, szExtraStyles)
{
	return OpenTag("img", szID, szSRC, nX, nY, nWidth, nHeight, nZ, "position:absolute;" + szExtraStyles, null, szExtra) + CloseTag("img");
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function UpdateCheckButtonColor(ButtonId, bChecked) {
    var eButton = document.getElementById(ButtonId);
    if (eButton) {
        eButton.style.backgroundColor = bChecked ? "#005500" : "#333333";
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Button
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function OnButtonDown(eButton) {   
    eButton.style.backgroundColor = "#888888";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function OnButtonUp(eButton) {    
    eButton.style.backgroundColor = "#333333";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function AttachButton(ButtonID, OnClickFunc) {
    var eButton = document.getElementById(ButtonID);
    if (eButton) {
        eButton.fnMouseDown = function() { OnButtonDown(eButton); OnClickFunc(); var func = function(eButton) { return function() { OnButtonUp(eButton); } } (eButton); t = setTimeout(func, 200); };
        AddEvent(ButtonID, "mousedown", eButton.fnMouseDown);
        /*AddEvent(ButtonID, "click", OnClickFunc);
        AddEvent(ButtonID, "mousedown", function() { OnButtonDown(eButton) });
        AddEvent(ButtonID, "mouseup", function() { OnButtonUp(eButton) });
        AddEvent(ButtonID, "mouseout", function() { OnButtonUp(eButton) });
        */
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function DetachButton(ButtonID, OnClickFunc) {
    var eButton = document.getElementById(ButtonID);
    if (eButton) {
        DetachEvent(ButtonID, "mousedown", eButton.fnMouseDown);
        eButton.fnMouseDown = null;
        /*DetachEvent(ButtonID, "click", OnClickFunc);
        DetachEvent(ButtonID, "mousedown", function() { OnButtonDown(eButton) });
        DetachEvent(ButtonID, "mouseup", function() { OnButtonUp(eButton) });
        DetachEvent(ButtonID, "mouseout", function() { OnButtonUp(eButton) });*/
    }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Multi-Select Check
///////////////////////////////////////////////////////////////////////////////
// ButtonIdBase will be postfix by 1..nNumberOfCheck
///////////////////////////////////////////////////////////////////////////////
function AttachMultiSelectCheck(ButtonIdBase, nNumberOfCheck, OnClickFunc) {
    for (var i = 1; i <= nNumberOfCheck; i++) {   
        var eButton = document.getElementById(ButtonIdBase + i);  
        eButton._Enable = true;
        eButton._Selected = false;
        AddEvent(ButtonIdBase + i, "mousedown", function(i) { return function() { OnClickFunc(i); } } (i));
    }
}

///////////////////////////////////////////////////////////////////////////////
function DettachMultiSelectCheck(ButtonIdBase, nNumberOfCheck, OnClickFunc) {
    for (var i = 1; i <= nNumberOfCheck; i++) {
        DetachEvent(ButtonIdBase + i, "mousedown", function(i) { return function() { OnClickFunc(i); } } (i));
    }
}

///////////////////////////////////////////////////////////////////////////////
function _UpdateMultiSelectCheckButton(eButton) {
    if (eButton._Enable) {
        eButton.style.backgroundColor = eButton._Selected ? "#005500" : "#333333";
        if (eButton.firstElementChild != undefined) {
            eButton.firstElementChild.style.color = "#dddddd";
        }
    }
    else {
        eButton.style.backgroundColor = "#111111";
        if (eButton.firstElementChild != undefined) {
            eButton.firstElementChild.style.color = "#555555";
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
function UpdateMultiSelectCheck(ButtonIdBase, nNumberOfCheck, nSelectedId) {
    for (var i = 1; i <= nNumberOfCheck; i++) {
        var eButton = document.getElementById(ButtonIdBase + i);
        if (eButton) {
            eButton._Selected = i == nSelectedId;
            _UpdateMultiSelectCheckButton(eButton);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
function EnableMultiSelectCheck(ButtonIdBase, nNumberOfCheck, bEnabled) {
    for (var i = 1; i <= nNumberOfCheck; i++) {
        var eButton = document.getElementById(ButtonIdBase + i);
        if (eButton) {
            eButton._Enable = bEnabled;
            _UpdateMultiSelectCheckButton(eButton);           
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
function IsMultiSelectCheckEnabled(ButtonIdBase, nNumberOfCheck) {
    for (var i = 1; i <= nNumberOfCheck; i++) {
        var eButton = document.getElementById(ButtonIdBase + i);
        if (eButton) {
            return eButton._Enable;            
        }
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////
function ShowDiv(DivName) {
    var eDiv = document.getElementById(DivName);
    if (eDiv) {
        eDiv.style.visibility = 'visible';
    }
}

///////////////////////////////////////////////////////////////////////////////
function HideDiv(DivName) {
    var eDiv = document.getElementById(DivName);
    if (eDiv) {
        eDiv.style.visibility = 'hidden';
    }
}

///////////////////////////////////////////////////////////////////////////////
function ShowDivs(DivPrefix, FirstId, LastId) {
	for(var id = FirstId; id <= LastId; id++) {
		var eDiv = document.getElementById(DivPrefix + id);
		if (eDiv) {
			eDiv.style.visibility = 'visible';				
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
function HideDivs(DivPrefix, FirstId, LastId) {
	for(var id = FirstId; id <= LastId; id++) {
		var eDiv = document.getElementById(DivPrefix + id);
		if (eDiv) {
			eDiv.style.visibility = 'hidden';				
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Cookies
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function CreateCookie(name, value, days) {
    if (days) {
        var date = new Date();
        date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
        var expires = "; expires=" + date.toGMTString();
    }
    else var expires = "";
    document.cookie = name + "=" + encodeURIComponent(value) + expires + "; path=/";
}

///////////////////////////////////////////////////////////////////////////////
function ReadCookie(name) {
    var nameEQ = name + "=";
    var ca = document.cookie.split(';');
    for (var i = 0; i < ca.length; i++) {
        var c = ca[i];
        while (c.charAt(0) == ' ') c = c.substring(1, c.length);
        if (c.indexOf(nameEQ) == 0) return decodeURIComponent(c.substring(nameEQ.length, c.length));
    }
    return null;
}

///////////////////////////////////////////////////////////////////////////////
function EraseCookie(name) {
    CreateCookie(name, "", -1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Helpers
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function ConvertLinearTodB(usLinear) {
    if (usLinear == 0) {
        return -144.5;
    }
    return 20 * Math.log10(usLinear / 65535);
}


///////////////////////////////////////////////////////////////////////////////
//Color2Hex('rgba(255, 136, 17, 0.5)'); // '#ff8811'
//Color2Hex('rgb(255, 136, 17)'); // '#ff8811'
//Color2Hex('#ff8811'); // '#ff8811'
//Color2Hex('#f81'); // '#f81'
///////////////////////////////////////////////////////////////////////////////
function Color2Hex(c) {
    var m = /rgba?\((\d+), (\d+), (\d+)/.exec(c);
    return m ? '#' + (m[1] << 16 | m[2] << 8 | m[3]).toString(16): c;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
var BrowserDetect =
{
	init: function () {
		this.browser = this.searchString(this.dataBrowser) || "An unknown browser";
		this.version = this.searchVersion(navigator.userAgent)
			|| this.searchVersion(navigator.appVersion)
			|| "an unknown version";
		this.OS = this.searchString(this.dataOS) || "an unknown OS";
        this.Mobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|BB|PlayBook|IEMobile|Windows Phone|Kindle|Silk|Opera Mini/i.test(navigator.userAgent);
	},
	searchString: function (data) {
		for (var i=0;i<data.length;i++)	{
			var dataString = data[i].string;
			var dataProp = data[i].prop;
			this.versionSearchString = data[i].versionSearch || data[i].identity;
			if (dataString) {
				if (dataString.indexOf(data[i].subString) != -1)
					return data[i].identity;
			}
			else if (dataProp)
				return data[i].identity;
		}
	},
	searchVersion: function (dataString) {
		var index = dataString.indexOf(this.versionSearchString);
		if (index == -1) return;
		return parseFloat(dataString.substring(index+this.versionSearchString.length+1));
	},
	dataBrowser: [
		{
			string: navigator.userAgent,
			subString: "Chrome",
			identity: "Chrome"
		},
		{ 	string: navigator.userAgent,
			subString: "OmniWeb",
			versionSearch: "OmniWeb/",
			identity: "OmniWeb"
		},
		{
			string: navigator.vendor,
			subString: "Apple",
			identity: "Safari",
			versionSearch: "Version"
		},
		{
			prop: window.opera,
			identity: "Opera",
			versionSearch: "Version"
		},
		{
			string: navigator.vendor,
			subString: "iCab",
			identity: "iCab"
		},
		{
			string: navigator.vendor,
			subString: "KDE",
			identity: "Konqueror"
		},
		{
			string: navigator.userAgent,
			subString: "Firefox",
			identity: "Firefox"
		},
		{
			string: navigator.vendor,
			subString: "Camino",
			identity: "Camino"
		},
		{		// for newer Netscapes (6+)
			string: navigator.userAgent,
			subString: "Netscape",
			identity: "Netscape"
		},
		{
			string: navigator.userAgent,
			subString: "MSIE",
			identity: "Explorer",
			versionSearch: "MSIE"
		},
		{
			string: navigator.userAgent,
			subString: "Gecko",
			identity: "Mozilla",
			versionSearch: "rv"
		},
		{ 		// for older Netscapes (4-)
			string: navigator.userAgent,
			subString: "Mozilla",
			identity: "Netscape",
			versionSearch: "Mozilla"
		}
	],
	dataOS : [
		{
			string: navigator.platform,
			subString: "Win",
			identity: "Windows"
		},
		{
			string: navigator.platform,
			subString: "Mac",
			identity: "Mac"
		},
		{
			   string: navigator.userAgent,
			   subString: "iPhone",
			   identity: "iPhone/iPod"
	    },
		{
			string: navigator.platform,
			subString: "Linux",
			identity: "Linux"
		}
	]

};
BrowserDetect.init();



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// rickshaw
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
function Rickshaw_CreateDivGraph(Graph, szName, css_class_prefix_name, nX, nY, nWidth, nHeight, nZIndex) {    
    var szHTML = "";
    var nXAxis_height = 20;
    var nYAxis_width = 40;
    var nLegend_height = 40;
    var nLegend_width = 250;

    Graph.nX = nYAxis_width;
    Graph.nY = 10;
    Graph.nGraphWidth = nWidth - nYAxis_width;
    Graph.nGraphHeight = nHeight - nXAxis_height - Graph.nY;// - nLegend_height ;

    szHTML += OpenTag("div", szName + "_container", null, nX, nY, nWidth, nHeight, nZIndex, "", css_class_prefix_name + "common " + css_class_prefix_name + "container");
        
    ///////////////////////////////////////////////////////////////////////////////
    szHTML += OpenTag("div", szName + "_y_axis", null, 0, Graph.nY, nYAxis_width, Graph.nGraphHeight, nZIndex, "", css_class_prefix_name + "common " + css_class_prefix_name + "y_axis");
    szHTML += CloseTag("div");
            
    ///////////////////////////////////////////////////////////////////////////////
    szHTML += OpenTag("div", szName + "_chart", null, Graph.nX, Graph.nY, Graph.nGraphWidth, Graph.nGraphHeight, nZIndex, "", css_class_prefix_name + "common " + css_class_prefix_name + "chart");
    szHTML += CloseTag("div");

    ///////////////////////////////////////////////////////////////////////////////
    szHTML += OpenTag("div", szName + "_x_axis", null, Graph.nX, Graph.nY + Graph.nGraphHeight, Graph.nGraphWidth, nXAxis_height, nZIndex, "", css_class_prefix_name + "common " + css_class_prefix_name + "x_axis");
    szHTML += CloseTag("div");
        
    ///////////////////////////////////////////////////////////////////////////////
    szHTML += OpenTag("div", szName + "_legend", null, Graph.nX + 10, Graph.nY + 10, nLegend_width, nLegend_height, nZIndex, "", css_class_prefix_name + "common " + css_class_prefix_name + "legend");
    szHTML += CloseTag("div");

    ///////////////////////////////////////////////////////////////////////////////
    szHTML += CloseTag("div");
    return szHTML;
}

///////////////////////////////////////////////////////////////////////////////
function Rickshaw_CreateGraph(Graph, szName, series_info, time_interval, value_min, value_max) {
        
    // instantiate our graph!
    Graph.graph = new Rickshaw.Graph({
        element: document.getElementById(szName + "_chart"),
        width: Graph.nGraphWidth,
        height: Graph.nGraphHeight,
        renderer: 'line',
        interpolation: 'linear',
        min: value_min,
        max: value_max,
        series: new Rickshaw.Series.FixedDuration(series_info, undefined, {
            timeInterval: time_interval,
            maxDataPoints: 100,
            timeBase: new Date().getTime() / 1000,
        })
    });

    Graph.xAxis = new Rickshaw.Graph.Axis.X({
        graph: Graph.graph,
        width: Graph.nGraphWidth,
        orientation: 'bottom',
        tickFormat: function (x) { return (Math.floor(x / 60) % 60) + ":" + (x % 60); },
        element: document.getElementById(szName + "_x_axis")
    });

    Graph.yAxis = new Rickshaw.Graph.Axis.Y({
        graph: Graph.graph,
        orientation: 'left',
        element: document.getElementById(szName + "_y_axis")
    });

    /*Graph.hoverDetail = new Rickshaw.Graph.HoverDetail( {
                graph: Graph.graph,
                formatter: function(series, x, y) {
                    var date = '<span class="date">' + new Date(x * 1000).toLocaleString() + '</span>';
                    var swatch = '<span class="detail_swatch" style="background-color: ' + series.color + '"></span>';
                    var content = swatch + series.name + ": " + parseInt(y) + " [" + series.unit + "]" + '<br>' + date;
                    return content;
                },
                xFormatter: function(x) {
                    return new Date( x * 1000 ).toLocaleString();
                    }
            } );*/


    Graph.legend = document.getElementById(szName + "_legend");

    var Hover = Rickshaw.Class.create(Rickshaw.Graph.HoverDetail, {

        render: function (args) {

            Graph.legend.innerHTML = args.formattedXValue;

            args.detail.sort(function (a, b) { return a.order - b.order }).forEach(function (d) {

                var line = document.createElement('div');
                line.className = 'line';
                line.style.margin = '0 0 0 5px';

                var swatch = document.createElement('div');
                swatch.className = 'swatch';
                swatch.style.backgroundColor = d.series.color;
                swatch.style.display = 'inline-block';
                swatch.style.width = '10px';
                swatch.style.height = '10px';
                swatch.style.margin = '0 8px 0 0';

                var label = document.createElement('div');
                label.className = 'label';
                label.style.display = 'inline-block';
                label.innerHTML = d.name + ": " + d.formattedYValue;

                line.appendChild(swatch);
                line.appendChild(label);

                Graph.legend.appendChild(line);

                var dot = document.createElement('div');
                dot.className = 'dot';
                dot.style.top = Graph.graph.y(d.value.y0 + d.value.y) + 'px';
                dot.style.borderColor = d.series.color;

                this.element.appendChild(dot);

                dot.className = 'dot active';

                this.show();

            }, this);
        }
    });

    Graph.hoverDetail = new Hover({
        graph: Graph.graph,
        /*formatter: function(series, x, y) {
            var date = '<span class="date">' + new Date(x * 1000).toLocaleString() + '</span>';
            var swatch = '<span class="detail_swatch" style="background-color: ' + series.color + '"></span>';
            var content = swatch + series.name + ": " + parseInt(y) + " [" + series.unit + "]" + '<br>' + date;
            return content;
        },*/
        xFormatter: function (x) {
            return new Date(x * 1000).toLocaleString();
        }
    });

    /*Graph.legend = new Rickshaw.Graph.Legend( {
        graph: Graph.graph,
        naturalOrder: true,
        element: document.getElementById(szName + "_legend")	        
    } );*/


    /*Graph.shelving = new Rickshaw.Graph.Behavior.Series.Toggle( {
        graph: Graph.graph,
        legend: Graph.legend
    } );*/

    Graph.graph.render();

        
    /*
    $('.x_ticks_d3 text').css('opacity', '1.0');//fix text opacity
$('#x_axis text').css('fill', 'white');//text color
$('.x_ticks_d3 .tick').css('stroke-width', '0px');//text smoothing
$('#x_axis path').css('opacity', '0');//remove line or
$('#x_axis path').css('stroke', 'white');//change line color
*/

    // fake variable to be able to call update function for hover
    var e = {
        offsetX: 0,
        offsetY: 0,
        target: { nodeName: 'path' }
    };
    Graph.hoverDetail.update(e);
}

///////////////////////////////////////////////////////////////////////////////
function Rickshaw_DestroyGraph(Graph) {
    delete Graph.legend;
    delete Graph.hoverDetail;
    delete Graph.xAxis;
    delete Graph.yAxis;
    delete Graph.graph;
}