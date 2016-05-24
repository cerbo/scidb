var x = -1;             // Session ID
var cancelled = false;  // cancel flag

var FILE;
var FILEOBJ;


// This fixes a nasty IE 8 bug (by appending changing query parameters):
$.ajaxSetup({
    cache: false
});

function cancel()
{
  cancelled = true;
  $("#result")[0].innerHTML = "<pre>Query cancellation in process...</pre>";
  $.get("/cancel?id="+x+"&auth="+getCookie("authtok"));
  $("#querycontainer").spin(false);
}

// val must be true/false
function lockstate(val)
{
  if(val)
  {
    $("#result")[0].innerHTML = "<pre>Wait...</pre>";
    $("#querycontainer").spin();
    $("#exq").addClass("disabled");
    $("#exqn").addClass("disabled");
    $("#uplo").addClass("disabled");
    $("#can").removeClass("disabled");
  } else {
    $("#querycontainer").spin(false);
    $("#exq").removeClass("disabled");
    $("#exqn").removeClass("disabled");
    $("#uplo").removeClass("disabled");
    $("#can").addClass("disabled");
  }
}

function execute_query(ret)
{
  cancelled=false;
  lockstate(true);

$.get(
  "/new_session?auth="+getCookie("authtok"),
  function(data){
    x = parseInt(data); // session ID
    var sq = $("#query")[0].value;
    var q = encodeURIComponent(sq);
    var urix = "/execute_query?id="+x+"&query="+q+"&save=dcsv&auth="+getCookie("authtok");
    if(ret==0)
      urix = "/execute_query?id="+x+"&query="+q+"&auth="+getCookie("authtok");
    var urir = "/read_lines?id="+x+"&n="+numlines.value+"&auth="+getCookie("authtok");
    var rel = "/release_session?id="+x+"&auth="+getCookie("authtok");

    $.get(urix)
     .fail(function(z){$("#result")[0].innerHTML = "<pre>" +
        z.responseText.replace(">","&gt;").replace("<","&lt;")
        + "</pre>";
        lockstate(false);
     })
     .done(function(z)
     {
       if(ret==0)
       {
         $("#result")[0].innerHTML = "<pre>SciDB Query ID " + z + " complete.</pre>";
         $.get(rel);
       } else
       {
// Read the output
         $.get(urir,
           function(z)
           {
             var gt = />/g;
             var lt = /</g;
             $("#result")[0].innerHTML = "<pre>" + 
               z.replace(gt,"&gt;").replace(lt,"&lt;") + "</pre>";
             lockstate(false);
           }).always(function(z){$.get(rel);});
       }
         lockstate(false);
     })

  })
  .fail(function()
  {
    $("#result")[0].innerHTML = "SESSION ERROR!";
    lockstate(false);
  });
}


function do_upload()
{
  var gt = />/g;
  var lt = /</g;
  var schema = $("#arraySchema").val();
  var dims = $("#arraySchema").val().replace(/.*=/,"").replace(/]/,"").replace(/:/,",").split(",");
  var start = parseInt(dims[0]);
  var chunkSize = parseInt(dims[2]);
  
  $.get(
    "/new_session?auth="+getCookie("authtok"),
    function(data){
      lockstate(true);
      x = parseInt(data); // session ID
      var rel = "/release_session?id="+x+"&auth="+getCookie("authtok");
      var urix = "/upload_file?id="+x+"&auth="+getCookie("authtok");

      var arrayName = $("#arrayName").val();

      var n = 0;
      if($("#header").prop("checked"))
      {
        n = 1;
      }
      var ner = parseInt($("#errorlimit").val());

      var formData = new FormData();
      formData.append('file', FILEOBJ);

      var xhr = new XMLHttpRequest();

      xhr.onreadystatechange = function(e) {
        if (this.readyState == 4 && this.status == 200) {
          var urir = "/loadcsv?id="+x+"&nerr="+ner+"&schema="+schema+"&name="+arrayName+"&head="+n+"&auth="+getCookie("authtok");
          $.get(urir, function(z) {
               $("#query")[0].value = "scan(" + arrayName + ")";
               $("#result")[0].innerHTML = "<pre>OK</pre>";
               execute_query(1);
             }).fail(function(z) {
               $("#result")[0].innerHTML = "<pre>" +
               z.responseText.replace(">","&gt;").replace("<","&lt;")
               + "</pre>";
               $.get(rel);
             }).always(function(z){
            lockstate(false);
           });
        }
      };

      xhr.open("POST", urix, false);
      xhr.send(formData);

    }).always(function(z){lockstate(false)});
}

function handleFileSelect(evt) {
  var files = evt.target.files; // FileList object
  var f = files[0];
  FILEOBJ = files[0];
  var reader = new FileReader();
  // Closure to capture the file information.
  reader.onload = (function(theFile) {
    return function(e) {
        FILE = e.target.result.replace(/\r/g,""); // Windows
        var firstline = FILE.substring(0,FILE.indexOf("\n"));
        if($("#header").prop("checked"))
        {
          FILE = FILE.substring(FILE.indexOf("\n")+1,FILE.length);
        }
        var m = (FILE.match(/\n/g)||[]).length;  // lines
        var n = firstline.split(",").length;     // attributes
        var attr = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".split("");
        if($("#header").prop("checked"))
        {
          attr = firstline.replace(/"/g,"").split(",");  // header for attribute names
        }
        var schema = "<";
        for(var i=0;i < Math.min(n-1,52); i++)
        {
          schema = schema + attr[i] + ":string, ";
        }
        schema = schema + attr[i] + ":string>";
        schema = schema + "[row=0:" + (m-1) + "," + Math.min(m,10000) + ",0]";
        $("#arraySchema").val(schema);
    }
  })(f);

  reader.readAsText(f);
}

$("#query").keyup(function (event)
{
  if (event.keyCode == 13 && event.shiftKey)
  {
    event.preventDefault();
    event.stopPropagation();
    execute_query(1);
   }
});
$("#query").keypress(function (event)
{
  if (event.keyCode == 13 && event.shiftKey)
  {
    return false;
  }
});

$(document).ready(function()
  {
    $("#query").focus();
    $('input[id=fup]').change(function() {
      $('#pretty-input').val($(this).val().replace("C:\\fakepath\\", ""));
    });
    $("#can").addClass("disabled");
    document.getElementById('fup').addEventListener('change', handleFileSelect, false);
  });
