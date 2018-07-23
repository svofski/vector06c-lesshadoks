<html><meta charset="UTF-8"><head><title>Конфигурация ПЛИС</title>
<script type="text/javascript" src="../140medley.min.js"></script>
<script type="text/javascript" src="../util.js"></script>
<link rel="stylesheet" type="text/css" href="../style.css">
<script type="text/javascript">

var xhr=j();

function doRefresh() {
    xhr.open("GET", "");
    xhr.onreadystatechange=function() {
        if (xhr.readyState==4 && xhr.status>=200 && xhr.status<300) {
            window.setTimeout(function() {
                    location.reload(true);
                    }, 3000);
        }
    }
    //ToDo: set timer to 
    xhr.send();
}

function setProgress(amt) {
    if (amt === false) {
        $("#progressbarcontainer").style.display = "none";
    } else {
        $("#progressbarcontainer").style.display = "block";
        $("#progressbarinner").style.width=String(amt*200)+"px";
    }
}

function doUpload() {
    $("#remark").innerHTML = "";
    var f=$("#file").files[0];
    if (typeof f=='undefined') {
        $("#remark").innerHTML="Can't read file!";
        return
    }
    xhr.open("POST", "upload");
    xhr.setRequestHeader("X-FileName", f.name);
    xhr.setRequestHeader("Content-Disposition", "filename=\"" + f.name + "\"");
    xhr.onreadystatechange=function() {
        if (xhr.readyState==4 && xhr.status >= 200 && xhr.status <= 400) {
            setProgress(1);
            $("#remark").innerHTML = xhr.responseText;
            setProgress(false);
            doDirectory();
        }
    }
    if (typeof xhr.upload.onprogress != 'undefined') {
        xhr.upload.onprogress=function(e) {
            setProgress(e.loaded / e.total);
        }
    }
    xhr.send(f);
    return false;
}

function doDelete() {
    $("#remark").innerHTML = "";
    var inputs = document.getElementsByTagName("input");
    var del = [];
    for (var i = 0; i < inputs.length; ++i) {
        if (inputs[i].type != "checkbox" || inputs[i].value != "nax") continue;
        if (inputs[i].checked) {
            del.push(parseInt("0x" + inputs[i].name));
        }
    }

    xhr.open("POST", "delete");
    xhr.setRequestHeader("X-SPIFFS-ids", del.join(";"));
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status >= 200 && xhr.status <= 300) {
            //doRefresh();
            doDirectory();
        }
    };
    xhr.send();
    return false;
}

function selectboot(name) {
    xhr.open("POST", "bootname");
    xhr.setRequestHeader("X-FileName", name);
    xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status >= 200 && xhr.status <= 300) {
            doDirectory();
        }
    };
    xhr.send();
    return false;
}

function doDirectory() {
    xhr.open("GET", "dir");
    xhr.onreadystatechange=function() {
        if (xhr.readyState==4 && xhr.status>=200 && xhr.status<300) {
            var dobj = JSON.parse(xhr.responseText);
            var txt = "";
            var boot = dobj.info.boot;
            for (var i = 0; i < dobj.files.length; ++i) {
                if (dobj.files[i].name.charAt(0) === ".") continue;
                var hx = Util.hex16(dobj.files[i].fsid);

                txt += '<input type=checkbox name="' + hx + '" value="nax"/>';
                txt += '<div class="dirline"';
                txt += ' onclick=\'selectboot("' + dobj.files[i].name + '");\'>';
                // 👢
                txt += (dobj.files[i].name == boot ? " * " : "   ") +
                    Util.pad(" ".repeat(36), dobj.files[i].name, false) +
                    Util.pad(" ".repeat(8), dobj.files[i].size.toString(), true) 
                    + "</div><br/>";
            }
            txt += "<br/>";
            txt += Util.pad(" ".repeat(24), dobj.info.used, true) + " bytes, " +
                (dobj.info.total-dobj.info.used).toString() + " bytes free";
            txt += "<div class='bootinfo'>Активная конфигурация ПЛИС: " + 
                boot + "</div>";
            $("#dir").innerHTML=txt;
            setProgress(false);
        }
    }
    xhr.send();
}

window.onload=function(e) {
    doDirectory();
}

</script>
</head>
<body>
<a id="up" href="..">..</a>
<div id="main">
<h1>Прошивки Шадков</h1>

Список файлов:
<pre id="dir">
Loading...
</pre>

<div id="delete">
<input type="submit" value="Удалить выделенные файлы" onclick="doDelete();"/>
</div> <!-- delete -->

<div id="upload">
<input type="file" id="file" />
<input type="submit" value="Закачать" onclick="doUpload();" />
<div id="progressbarcontainer">
<div id="progressbar"><div id="progressbarinner">C O S M O G O L&nbsp;&nbsp;9 9 9</div></div>
</div> <!-- upload -->
</div>
<div id="remark">SPIFFS %mount_status%</div>
</div>
</body></html>
