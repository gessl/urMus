function ucwords(str) {
  return (str + '').replace(/^(.)|\s(.)/g, function ($1) {
    return $1.toUpperCase();
  });
}

function pathselect(dirtype) {
  if (dirtype == "root") return "";
  else return "/Documents";
}

var tabIndexMap = [];
var cm = [];
var saved = [];
var tabLabels = [];

$(window).bind('beforeunload', function(){
  window.saveAllFiles();
/*
var opened_files = ""
 for (var i=0; i<window.tableIndexMap.length; i++){
    opened_files += window.tabLabels[window.tabIndexMap[i]];
    opened_files += "\n";
  }
    //*/
  return 'Do you want to leave the page?';

});

window.saveAllFiles = function(){
  var opened_file = "";
  for (var i=0; i<tabIndexMap.length; i++){
    opened_file = tabLabels[tabIndexMap[i]];  
    opened_file = opened_file.substring(0,opened_file.indexOf(".lua")) + ".draft.lua"
    $.ajax({
      async : false,
      type: 'POST',
      url: '/upload_script',
      data: {
        file: opened_file,
        contents: cm[tabIndexMap[i]].getValue()
      },
      error: function () {
        alert("Couldn't upload " + opened_file);
      }
    });
  };
}
/*
$(window).bind('onunload',function(){
  for (var i=0; i<tableIndexMap.length; i++){
    opened_file = tabIndexMap[i]
    alert(opened_file);
   
  }
});
*/

$(document).ready(function () {
  
  var namespace_setfenv = "setfenv(1, #{namespace})\n"; 

  //jQuery.makeArray();
  liveTabCounter = 0;

  $("#tabs").tabs();
  var tabs = $("#tabs").tabs();
  var vtabs = $("#vtabs").tabs();
//  vtabs.css("height",15);

  var files = $("#files"),
  gallery_files = $("#gallery_files"),
  s = $("#status"),
  player = $("#jquery_jplayer");
  player.jPlayer({
    nativeSupport: true,
    oggSupport: false,
    customCssIds: true,
    swfPath: 'js/jplayer'
  })
  .jPlayer("onSoundComplete", function () {
    this.element.jPlayer("play");
  });

  
  var adddialog = $("#adddialog").dialog({
    autoOpen: false,
    modal: true,
    buttons: {
      Add: function () {
        addTab();
        $(this).dialog("close");
      },
      Cancel: function () {
        $(this).dialog("close");
      }
   },
   close: function () {
      form[0].reset();
   }
  });
  var rdialog = $("#rdialog").dialog({
    autoOpen: false,
    modal: true,
    buttons: {
      Add: function () {
        $(this).dialog("close");
      },
      Cancel: function () {
        $(this).dialog("close");
      }
    },
    close: function () {
      form[0].reset();
    }
  });

  function add_expression(table, namespace, expression){

    var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, namespace)
    ajax_run_code(target_namespace + 'NPrint(ElemToXml(' + expression+'))')
    .done( function (data,status, xhr) {
      expression = expression.replace(/"/g, "'");
      var depth = 2;
      var element = xhr.responseXML.getElementsByTagName("e")[0];
      var type = element.getElementsByTagName("t")[0].firstChild.nodeValue;
      var value;
      if ( type == "number" || type == "boolean" ||  type == "string" ){
        value = " : "+  element.getElementsByTagName("v")[0].firstChild.nodeValue;
      }
      else
        value = "";

      var keyType = "expression";
      var strRow = "<tr data-depth="+depth+" class=\"expression expand\" keytype = \"" + keyType+ "\" type = \"" + type + "\" namespace = \"" + namespace +"\" id =\""+expression+"\" key = \""+expression+"\"><td>";
      for (var j=1; j< depth; j++)
        strRow+= "&nbsp;&nbsp;";
     // if ( element.getElementsByTagName("v")[0].firstChild.nodeValue != "null")
       // value = " : "+  element.getElementsByTagName("v")[0].firstChild.nodeValue;
      strRow +="<span class=\"tree\"></span>";
      strRow += "<input type = \"checkbox\" keytype = \"" + keyType + "\" id =\"checkbox_"+expression;
      strRow +="\">";
      strRow += expression;
      strRow +=" ("+type+") ";//
      strRow += "  <a urmusid = \""+expression+"\" class = \"livebutton\" namespace = \"" + namespace + "\">Live</a>";;
      strRow += "<span class = \"urmus_value\">";
      strRow += value;
      strRow += "</span>";
      strRow += "</td></tr>\n";
      $('#table_'+table).find("tr:first").after(strRow);

      $("#expdialog").dialog("close");
      msg('success', "Expression(" + expression + ") is successfully added to namespace(" + screen_name+"," + table + ")." + $('#table_'+namespace).find("tr:first").attr("id"));
   
    }
    ).fail(function (xhr, opts, errorThrown) {
      alert( "Expression cannot be added:" + xhr.responseText);
      msg ('error', "Expression cannot be added(" + namespace + ","+ table +"," + expression + "):" + xhr.responseText);

    });
      
  }

  var expdialog = $("#expdialog").dialog({
    autoOpen: false,
    modal: true,
    buttons: {
      Add: function () {
        add_expression("__"+screen_name, "__"+screen_name, $('#urmus_expression').val().trim());
      },
      Cancel: function () {
        $(this).dialog("close");
      }
    },
    close: function () {
      form[0].reset();
    }
  });

  expdialog.keypress(function(e) {
    if (e.keyCode == $.ui.keyCode.ENTER) {
      $(this).siblings('.ui-dialog-buttonpane').find('button:eq(0)').click();
      e.preventDefault();
    }
  });
                
  // addTab form: calls addTab function on submit and closes the dialog
  var form = adddialog.find("form").submit(function (event) {
    addTab();
    adddialog.dialog("close");
    event.preventDefault();
  });

  var tabTitle = $("#tab_title");
  var tabTemplate = "<li><a href='#{href}' id = '#{id}'>#{label}</a> <span class='ui-icon-close'><img src=./images/close.gif valign = middle></span></li>";
  var vtabTemplate = "<li><a href='#{href}' id = '#{id}'>#{label}</a></li>";
  var scroll_section_Template = "<div class = \"scroll_section\" id = \"variable_list_#{id}\">\n<table class = \"variable_list\" id=\"table_#{id}\" width = 900>\n<tr data-depth=\"1\" type = \"table\" class=\"expand\" id =\"#{id}\" newindex = 0 namespace =\"#{id}\">\n<td><input type = checkbox keytype=\"none\" id =\"checkbox_#{id}\">#{label}</td>\n</tr>\n</table>\n</div>";
  var tabCounter = 0;

  // actual addTab function: adds new tab using the input from the form above
  function addVTab(id, label) {
    var li = $(vtabTemplate.replace(/#\{href\}/g, "#" + id).replace(/#\{label\}/g, label ).replace(/#\{id\}/g, "vtab_"+id));
    if (vtabs.find(".ui-tabs-nav").find("li").length == 0)
      vtabs.find(".ui-tabs-nav").append(li);
    else
      vtabs.find(".ui-tabs-nav").find("li:first").after(li);
    var scroll_section = scroll_section_Template.replace(/#\{id\}/g, id).replace(/#\{label\}/g, label+ " namespace");
    vtabs.append("<div id='" + id + "'>" + scroll_section + "</div>");
    vtabs.tabs("refresh");

    $('#table_'+id).on('click', '.toggle', function () {
      //Gets all <tr>'s  of greater depth
      //below element in the table 
      var el = $(this);
      var tr = el.closest('tr'); //Get <tr> parent of toggle button
      addChildren(tr, false);
    });

    $('#table_'+id).on('click','.livebutton', function(){
      var lb = $(this)
      lb.toggleClass("down");
      poll(lb);
    });
  }
  function endsWith(str, suffix) {
      return str.indexOf(suffix, str.length - suffix.length) != -1;
  }
  
  function addTab() {
    var label = tabTitle.val() || "Tab" + tabCounter + ".lua";
    if (!endsWith(label,".lua"))
      label = label + ".lua";
    id = tabCounter;
    var li = $(tabTemplate.replace(/#\{href\}/g, "#" + id).replace(/#\{label\}/g, label).replace(/#\{id\}/g, "tab_"+id));
    tabs.find(".ui-tabs-nav").append(li);
    tabs.append("<div id='" + id + "'><textarea id='code" + tabCounter + "' name = 'code" + tabCounter + "'></textarea></div>");
    tabs.tabs("refresh");
    tabIndexMap.push(tabCounter);
    tabLabels.push(label);
    saved.push(false);
    var tempEditor = CodeMirror.fromTextArea(document.getElementById("code" + tabCounter), {
      lineNumbers: true,
      tabSize: 4,
      indentUnit: 4,
      indentWithTabs: true,
      smartIndent: true,
      autofocus: true,
      tabMode: "indent",
      matchBrackets: true,
      mode: "lua",
      extraKeys: {
        "Cmd-R": run_code_in_editor,
        "Ctrl-R": run_code_in_editor
      }
    });
    cm.push(tempEditor);
    tabCounter++;
    tabs.tabs('select', tabIndexMap.length);
    cm[cm.length - 1].refresh();
    cm[cm.length - 1].focus();
  }
  
  // addTab button: just opens the dialog
  $("#add_tab").button().click(function () {
    tabTitle.val("");
    adddialog.dialog("open");
  });

  // close icon: removing the tab on click
  $("#tabs span.ui-icon-close").live("click", function () {
    if (confirm('Are you sure you want to delete this tab?')) {

      var panelId = $(this).closest("li").remove().attr("aria-controls");

      for (i=0; i<tabIndexMap.length; i++)
      {
        if(tabIndexMap[i]==parseInt(panelId)){
          cm[tabIndexMap[i]] = null;
          tabLabels[tabIndexMap[i]] = null;
          saved[tabIndexMap[i]] = null;
          tabIndexMap.splice(i,1);
          break;
        }
      }
     $("#" + panelId).remove();

     tabs.tabs("refresh");
     tabs.tabs('select', 0);
    }
  });

  // run variable list / chat helper functions.
  var displayed_chat_index;
  var chatboxState = false;
  var theFirstUser = false;

  function ajax_run_code(script) {

    return $.ajax({
          async : false,
          type: 'POST',
          url: '/eval',
          data: {
            code: script
          }
    }).fail(function(xhr, opts) {
      msg('error', xhr.responseText )
    });

  }

  function load__urMus_variable_list(){
    $.get('/open_file', {
    file: '__urmus_variable_list.lua',
    dirtype: 'root',
    async : false
    }, 
    function (script) {
     ajax_run_code(script)
     .done( function (data,status, xhr) {
        msg('success', "Variable List Ready.");

          ajax_run_code('NPrint(ElemToXml(__urMus__chat__index))')
          .done(function (data,status, xhr) {
            displayed_chat_index  = parseInt(xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue);
            if (displayed_chat_index == 0){
              // if you are the very first coder in this urMus? 
              ajax_run_code('__urMus_chat_register_id("'+screen_name+'")')
              .done( function (data,status, xhr) {
                if ($("#__" + screen_name).length == 0)
                {
                  addVTab("__" + screen_name,screen_name );
                }
                vtabs.tabs('select', 0);
              });
              chatboxState = false;
              theFirstUser = true;
              displayed_chat_index = 0;
            }
            else{
              chatboxState = true;
              theFirstUser = false;
              c_dialog.dialog("open");
              $('#open_chatbox').hide();
            }
            updateLog();

          }).fail(function (xhr, opts, errorThrown){
          msg('error', xhr.responseText);
          });
      })
     .fail(function (xhr, opts) {
        msg('error', "This page requires __urmus_variable_list.lua. :" + xhr.responseText);
      });
    });
  }
  
  /* beginning of chat interface */
  var box = null;
  var screen_name = "urMus";

  var c_dialog = $("#chat_dialog").dialog({
    autoOpen: false,
    modal: true,
    closeOnEscape: false,
    buttons: {
      Okay: function () {

        var temp_screen_name = $('#chat_name').val();
       // $("#chat_div").chatbox("option", "user", temp_screen_name);

        // from the 2nd users joined 
        if ( theFirstUser ) {
           // the very first user opened the chat box. 
          msg('success', "the first user joined.");
                                          
          ajax_run_code('__urMus_chat_rename_chat_id("'+temp_screen_name+'", "'+screen_name+'")')
          .done( function (data,status, xhr) {
            chatboxState = true;
            
            addVTab("__" + temp_screen_name,temp_screen_name );
            
            $("#__" + screen_name).remove();
            
            $("#vtab___urMus").parent().remove();
            screen_name = temp_screen_name;
            updateVariableList(true, "__" + screen_name);
            vtabs.tabs("refresh");
            vtabs.tabs('select', 0);
            openChatBox(temp_screen_name, theFirstUser);
            
            //ajax_run_code("__urMus_chat_post_message(\""+temp_screen_name+"\", \" joined.\",\"join\")");
            $("#chat_dialog").dialog("close");
          })
          .fail(function (xhr, opts, errorThrown){
            $('#open_chatbox').show();
          }); 
        }
        else{
          msg('success', "the another user joined2.");
          $('#broadcast_chat').button().hide();
          ajax_run_code('__urMus_chat_register_id("'+temp_screen_name+'")')
          .done( function (data,status, xhr) {
            addVTab("__" + temp_screen_name,temp_screen_name );
            vtabs.tabs('select', 0);
            var elements= xhr.responseXML.getElementsByTagName("e");
            if (elements.length > 1){
              addVTab("_G","Shared");
              updateVariableList(true,"_G");
              $('#share_button').button({ disabled: false });
            }  

            for(var i=0; i<elements.length; i++){
              var key = elements[i].getElementsByTagName("k")[0].firstChild.nodeValue;
              if (key!=temp_screen_name){
                addVTab("__" + key, key);
                updateVariableList(true,"__" + key);
              }
            }
            screen_name = temp_screen_name;
            openChatBox(temp_screen_name, theFirstUser);
            //ajax_run_code("__urMus_chat_post_message(\""+temp_screen_name+"\", \" joined.\",\"join\")");
            $("#chat_dialog").dialog("close");
          })
          .fail(function (xhr, opts, errorThrown){
            alert(xhr.responseText);
          //  $('#open_chatbox').show();
          });
        }
        // if existing id typed, it should be not opened? 
      } 
    },
    close: function () {
   //   if (!chatboxState)
     //   $('#open_chatbox').show();
    
//      $("#chat_dialog").dialog("close");
    //  msg('success', "this is running right?.");
    }

  });
  c_dialog.keypress(function(e) {
    if (e.keyCode == $.ui.keyCode.ENTER) {
      $(this).siblings('.ui-dialog-buttonpane').find('button:eq(0)').click();
      e.preventDefault();
    }
  });

  function openChatBox(temp_screen_name, firstuser){
    
    ajax_run_code('NPrint(ElemToXml(__urMus__chat__index_for_new_join))')
    .done(function (data,status, xhr) {
      displayed_chat_index  = parseInt(xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue);
    });
    var button = "";
    box = $("#chat_div").chatbox({
      id:"chat_div",
      user: temp_screen_name,
      title : "urMus chat" ,//+ button,
      width : "100%",
      
      messageSent : function(id, user, message) {
        var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, "__" + screen_name)
    //ajax_run_code(target_namespace + cm[selected].getSelection())
    
        ajax_run_code(target_namespace + '\n__urMus_chat_post_message("'+user+'", "'+message+'","chat")')
        .done(function (data,status, xhr) {
        });
      }
    }); 
    $("#chat_div").chatbox("option", "boxManager").addBroadcast(function(){
        ajax_run_code('__urMus__broadcast_chat_toggle()');

    });
    /*;*/

/*
    ("#broadcast_button").click(function(){
        ajax_run_code('__urMus__broadcast_chat = not __urMus__broadcast_chat');

    });
*/

   // $("#chat_div").chatbox("option", "boxManager").broadcast();
     
    /*
    if (firstuser)
      $('#broadcast_chat').button().click(function(){
        ajax_run_code('__urMus__broadcast_chat = not __urMus__broadcast_chat');
      }); 
*/

    }


  $('#open_chatbox').button().click(function(){
    c_dialog.dialog("open");
    $(this).hide();
  });

  

  load__urMus_variable_list();


  var count = 0;
  var count2 = 0;
  var lostConnection = false;
  function requestChat(){

    if ( lostConnection == true)
    {
    // see if it respond : 
      ajax_run_code('NPrint(ElemToXml(0))')
      .done(function (data,status, xhr) {
        lostConnection = false;
        msg('success', "urMus reconnected!("+xhr.responseText+")");
//        alert("urMus reconnected! screen_name : " + screen_name);
        load__urMus_variable_list();

        updateLog();
        var tr = $('#table___'+screen_name).find("tr:first");
        tr.attr("newindex",0);
        var trNext = tr.next();
        var trCur;
        while(trNext.length > 0){
          trCur = trNext;
          trNext = trNext.next();
          trCur.remove();
        }
       // $('#table___'+screen_name).find("tr:first").attr("newindex",0);
       // updateVariableList(true,"__" + screen_name);
        //updateVariableList(true,"_G");
      }
      ).fail(function (data,status, xhr) {
        updateLog();
      }
      )
      return;

    }
    if (!chatboxState){
      ajax_run_code('NPrint(ElemToXml(__urMus__chat__index_for_new_join))')
      .done(function (data,status, xhr) {

        var idx_new_join = parseInt(xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue);
        if (idx_new_join>=1)
        {
         // msg("success", "numUser greater than 1");
          $('#chat_name').val(screen_name);
          chatboxState = true;
          $('#open_chatbox').hide();
          openChatBox(screen_name, theFirstUser);
          // ajax_run_code("__urMus_chat_post_message(\""+temp_screen_name+"\", \" joined.\",\"join\")");
        }
        updateLog();

      }).fail(function (xhr, opts, errorThrown){
        msg('error', "RequestChat() failed: urMus does not respond. ("+xhr.responseText+")");
        lostConnection = true;
        updateLog();
      });

      return;
    }
      

    count++;
    ajax_run_code('NPrint(__urMus_chat_get_messages_in_xml("'+  screen_name + '",' + (displayed_chat_index+1) +'))')
    .done(function (data,status, xhr) {
      var string = (new XMLSerializer()).serializeToString(xhr.responseXML);
      if (xhr.responseXML == undefined || xhr.responseXML.getElementsByTagName("chatxml").length == 0){
      //  alert(xhr.responseXML);
        alert("stoppped1 " + xhr.responseText);
        updateLog();
        return;
      }
      var something= xhr.responseXML.getElementsByTagName("chat");
      for (var i=0; i< something.length; i++){
        var element  = xhr.responseXML.getElementsByTagName("chat")[i];
        var type = element.getElementsByTagName("t")[0].firstChild.nodeValue;
        var index = parseInt(element.getElementsByTagName("i")[0].firstChild.nodeValue);
        var message = element.getElementsByTagName("m")[0].firstChild.nodeValue;
        var user = element.getElementsByTagName("u")[0].firstChild.nodeValue;
     
        displayed_chat_index  = index;
     //   if (user != screen_name)
        var shared = false;
        if ( type == "chat")
          $("#chat_div").chatbox("option", "boxManager").addMsg(user, message, (user == screen_name?"#960000":"#450096"));
        else if (type == "code"){
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#009600");
          if (user != screen_name)
            updateVariableList(true, "__"+user);
          if ($('#table__G').length > 0)
            updateVariableList(true, "_G");

        }else if (type == "join"){
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#969600");
          if (user != screen_name){
            if ($('#table__G').length == 0){
              addVTab("_G","Shared");
              updateVariableList(true, "_G");
              $('#share_button').button({ disabled: false });
            }
            addVTab("__" + user,user );
            updateVariableList(true, "__"+user);
          }

        }else if (type == "share"){
          shared = true;
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#004596");
        }
        else if (type == "remote"){
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#960096");
        }
        else if (type == "share_expression"){
          shared = true;
          var expression = element.getElementsByTagName("d")[0].firstChild.nodeValue;
          add_expression("_G", "__" + user, expression);
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#009696");
        }else{
          $("#chat_div").chatbox("option", "boxManager").addMsg(false, user + message, "#960045");

        }

        if(shared)
          updateVariableList(true, "_G");
      }
      updateLog();
    }).fail(function (xhr, opts, errorThrown){
      msg('error', "RequestChat() failed: urMus does not respond. ("+xhr.responseText+")");
      lostConnection = true;
      updateLog();
    });
  }
  var count_updateLog=0;

  function updateLog(){
    setTimeout(requestChat, 1000);
  }
 
 
  // end of chat interface

  
  function run_code_in_editor() {
    selected = tabIndexMap[tabs.tabs('option', 'selected')-1] // => 0
    var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, "__" + screen_name)
    ajax_run_code(target_namespace + cm[selected].getSelection())
    .done( function (data,status, xhr) {
        msg('success', "Ran \"" + cm[selected].getSelection().substring(0,20) + "...\" : " + xhr.responseText);
        updateVariableList(true,"__" + screen_name);
        ajax_run_code("__urMus_chat_post_message(\""+screen_name+"\", \" submitted code.\",\"code\")");
    })
    .fail(function(){
      updateVariableList(true,"__" + screen_name);
    });  
  }


 /* function printTable(table, tableid, depth){
      var elements = table.getElementsByTagName("e");
      var strRow = "";
      var i;
      for(var i=0; i<elements.length; i++){
      
      var key = elements[i].getElementsByTagName("k")[0].firstChild.nodeValue;
      
      if (key.match("^__"))
        continue;
      var keyType = elements[i].getElementsByTagName("kt")[0].firstChild.nodeValue;
      var type = elements[i].getElementsByTagName("t")[0].firstChild.nodeValue;
      var value = "";
      
      
      strRow += "<tr data-depth="+(depth+1)+" class=\"element expand\" keyType = \"" + keyType + "\" type = \"" + type + "\" id =\""+tableid;
      if (keyType == "number")
      strRow +="["+key+"]";
      else
      strRow +="."+key;
      strRow += "\" key = \""+key+"\"><td>";
      
      
      for (var j=0; j< depth+1; j++)
        strRow+= "&nbsp;&nbsp;";
      if ( type == "table" ){
      strRow +="<span class=\"toggle expand\"></span>";
      }
      else
      {
      if ( elements[i].getElementsByTagName("v")[0].firstChild.nodeValue != "null")
      value = " - "+  elements[i].getElementsByTagName("v")[0].firstChild.nodeValue
      strRow +="<span class=\"tree\"></span>";
      }
      
      strRow += "<input type = \"checkbox\" keytype = \"" + keyType + "\" id =\"checkbox_"+tableid;
      if (keyType == "number")
      strRow +="["+key+"]";
      else
      strRow +="."+key;
      strRow +="\">"

      
      if (keyType == "number")
      strRow+= "[" + key + "]";
      else
      strRow += key;
      strRow += " ("+type+")";//
      strRow += "<span class = \"urmus_value\">";
      strRow += value;
      strRow += "</span>";
      if (type == "number" || type == "string" || type == "boolean"){
        strRow += "  <a urmusid = \""+_id+"\" class = \"livebutton" +
        if (\">Live</a>";
      }
      strRow += "</td></tr>\n";
      
      }
      var ret_result = [];
      ret_result[0] = i;
      ret_result[1] = strRow;
      return ret_result;
  }
  */

  function updateOneTr(tr){
    var _id = tr.attr("id");
    var depth = tr.data('depth');
    var key = tr.attr("key");
    var keyType = tr.attr("keytype");
    var newindex = tr.attr("newindex");
    var exp = tr.hasClass("expression");
    var namespace = tr.attr("namespace")
    var target_namespace = "";
    if (exp)
      target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, namespace);  


   // ajax_run_code('NPrint(ElemToXml('+_id+'))')
   // .done()
    ajax_run_code(target_namespace + 'NPrint(ElemToXml('+_id+'))')
    .done(function (data,status, xhr) {
      var element = xhr.responseXML.getElementsByTagName("e")[0];
      var type = element.getElementsByTagName("t")[0].firstChild.nodeValue;
      var value = "";
      if ( type == "nil"){
        if ( tr.find('a.livebutton').hasClass("down"))
          tr.find('a.livebutton').removeClass("down");
        tr.remove();
        return;
      }
      var live = false;

      var strRow = "<tr data-depth="+depth+" class=\"element expand\" keytype = \"" + keyType+ "\" type = \"" + type + "\" namespace = \"" + namespace +"\" id =\""+_id+"\" key = \""+key+"\" newindex = " + newindex + "><td>";
      for (var j=1; j< depth; j++)
        strRow+= "&nbsp;&nbsp;";
      if ( type == "table" || type == "region" ){
        strRow +="<span class=\"toggle ";
        if (true){//tr.find('span')[0].hasClass('collapse')
          strRow+="collapse";
        }
        else{
          strRow+="expand" ;
        }
        strRow += "\"></span>";
      }
      else
      {
        if ( element.getElementsByTagName("v")[0].firstChild.nodeValue != "null")
          value = " : "+  element.getElementsByTagName("v")[0].firstChild.nodeValue;
        strRow +="<span class=\"tree\"></span>";
      }
      strRow += "<input type = \"checkbox\" keytype = \"" + keyType + "\" id =\"checkbox_"+_id;
      strRow +="\">"
      if (keyType == "number")
        strRow +="["+key+"]";
      else
        strRow +=key;
      strRow += " ("+type+")";//
      if (type == "number" || type == "string" || type == "boolean"){
        strRow += "  <a urmusid = \""+_id+"\" class = \"livebutton " + "\" namespace = \"" + namespace +"\"";
        if ( tr.find('a.livebutton').hasClass("down")){
          strRow += "down";
          live= true;
        }
        strRow +="\">Live</a>";
      }
      strRow += "<span class = \"urmus_value\">";
      strRow += value;
      strRow += "</span>";

      strRow += "</td></tr>\n";
      tr.after(strRow);
      //if ( tr.is(":hidden") )
   //   if ( tr.is(":hidden") )
     //   tr.next().hide();
      if(live)
        poll(tr.next().find('a.livebutton'));
      tr.remove();
    })
    .fail(function (xhr, opts, errorThrown) {
      if (xhr.responseText.indexOf("attempt to index field") >= 0){
        if ( tr.find('a.livebutton').hasClass("down"))
          tr.find('a.livebutton').removeClass("down");
        tr.remove();
        msg('error', "UpdateOneTr() error1: " + xhr.responseText);

      }
      else{
        msg('error', "UpdateOneTr() error2: " + xhr.responseText);
      }
    });
  }

  function updateNewElements(tr, hide){

    var _id = tr.attr("id");
    var _depth = tr.data('depth');
    var length = 0;
    var exists = "";
    var newindex = parseInt(tr.attr('newindex'));
    var namespace = tr.attr("namespace");
    /* sanity check 
        1. see if _id exists
        2. see if __newindex__length exists 
        3. return __newindex__length. 
    */
    $.ajax({ // see if _id exists 
      async: false,
      type: 'GET',
      url: '/eval',
      data: {
        code: 'NPrint(ElemToXml('+_id+'==nil))'
      },
      success:  function(data, status, xhr)  {
        exists = xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue;
      },
      error: function (xhr, opts, errorThrown) {
        msg('error', "Error4:" + xhr.responseText + ":" + errorThrown);
        return;
      }
    });
    if (exists == "true"){
      msg('error', "Error:" + _id+ " does not exists.");
      return;
    }

    $.ajax({
      async: false,
      type: 'GET',
      url: '/eval',
      data: {
        code: 'NPrint(ElemToXml('+_id+'.__newindex__length==nil))'
      },
      success:  function(data, status, xhr)  {
        exists = xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue;
      },
      error: function (xhr, opts, errorThrown) {
        msg('error', "Error5:" + xhr.responseText + ":" + errorThrown);
        return;
      }
    });
    if (exists == "true"){
      return;
    }
    
    $.ajax({
      async: false,
      type: 'GET',
      url: '/eval',
      data: {
        code: 'NPrint(ElemToXml('+_id+'.__newindex__length))'
      },
      success:  function(data, status, xhr)  {
        length = parseInt(xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue);
      },
      error: function (xhr, opts, errorThrown) {
        msg('error', "Error6:" +  _id + ":"+ xhr.responseText + ":" + errorThrown);
        return;
      }
    });

    if (length == 0)
      return;

    $.ajax({
      async: false,
      type: 'GET',
      url: '/eval',
      data: {
        code: 'NPrint(__newindex_table_to_xml('+_id+','+ (newindex+1) + '))'
      }  ,
      success: function (data,status, xhr) {
        var table = xhr.responseXML.getElementsByTagName("lua_table")[0];
        var elements = table.getElementsByTagName("e");
        for (var i=0; i< elements.length; i++){
          var keyType = elements[i].getElementsByTagName("t")[0].firstChild.nodeValue;
          var key = elements[i].getElementsByTagName("v")[0].firstChild.nodeValue;
      //  if (key.match("^__"))
      //      return;
          var strRow = "<tr data-depth="+(_depth+1)+" class=\"element expand\" keytype = \"" + keyType + "\" id =\""+_id;
          if (keyType == "number")
            strRow += "[" + key + "]";
          else
            strRow += "." + key;
          strRow += "\" key = \""+key + "\" ";
          strRow += "newindex = 0 namespace = \"" + namespace + "\"><td>updating failed...";
          strRow += "</td></tr>\n";
          tr.after(strRow);
          if (hide)
            tr.next().hide();
          updateOneTr(tr.next());
        }
        tr.attr('newindex', length);

      },
      error: function (xhr, opts, errorThrown) {
        msg('error', "Error7:" + xhr.responseText);
      }
    });
/*
    $.ajax({
      async: false,
      type: 'GET',
      url: '/eval',
      data: {
        code: _id+'.__newindex__length = 0'
      },
      success:  function(data, status, xhr)  {
      },
      error: function (xhr, opts, errorThrown) {
        msg('error', "Error8:" + xhr.responseText);
      }
    });
*/
  }

  function findChildren(tr) {
    var depth = tr.data('depth');
    return tr.nextUntil($('tr').filter(function () {
     return $(this).data('depth') <= depth;
   }));
  }

  function removeHighlight() {
      setTimeout(function() {
        $( "#effect" ).removeAttr( "style" ).hide().fadeIn();
      }, 1000 );
    };
 

  function updateVariableList(addNew, namespace){
    var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, "__" + screen_name);  

    var _id = $('#table_'+namespace).find("tr:first").attr("id");
    if (addNew)
      updateNewElements($('#table_'+namespace).find("tr:first"),false );

    $("tr.element[namespace=" + namespace + "]").each(function() {
      var tr = $(this);
      var namespace2 = tr.attr("namespace");
      if ( namespace != namespace2)
        msg("error", "namespace errror baby(" + namespace + "," + namespace2+")");
      var trs = findChildren(tr)
      if (trs.length > 0 && addNew){
        updateNewElements(tr,tr.next().is(":hidden"))
      }
      updateOneTr(tr);
    });

    $("tr.expression[namespace=" + namespace + "]").each(function(){
      tr = $(this);
      _id = tr.attr("id");
      var namespace2 = tr.attr("namespace");
      var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, namespace2);  

      $.ajax({
        async: false,
        type: 'GET',
        url: '/eval',
        data: {
          code: target_namespace + 'NPrint(ElemToXml('+_id+'))'
        },
        success: function (data,status, xhr) {
          var type =  xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("t")[0].firstChild.nodeValue;
          var value = " : "+  xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue;
          if (type == tr.attr("type"))
            tr.find("span.urmus_value").text(value);
          else{
            alert(type + ":"+ tr.attr("type"));
            updateOneTr(tr);
          }
        }
        ,
        error: function (xhr, opts, errorThrown) {
          msg("error", "update failed :" + _id + "," + target_namespace  + ":" + xhr.responseText);
          tr.find("a.urmusid").toggleClass('down');
        }
      });
    });

    $("#vdialog").effect("highlight", {}, 500, removeHighlight)
  }

  
    // callback function to bring a hidden box back
    
  function poll(lb){
    setTimeout(function(){

      var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, lb.attr("namespace"));  

      _id = lb.attr("urmusid");
      if (!lb.hasClass('down'))
        return;

      $.ajax({
        type: 'GET',
        url: '/eval',
        data: {
          code: target_namespace +'NPrint(ElemToXml('+_id+'))'
        }
      }).done(function (data,status, xhr) {
        var value = xhr.responseXML.getElementsByTagName("e")[0].getElementsByTagName("v")[0].firstChild.nodeValue;
        if (value != "null"){
          value = " : "+  value;
          lb.parent().find("span.urmus_value").text(value);
        }
        poll(lb);
      })
      .fail(function (xhr, opts, errorThrown) {
        msg("error", "update failed :" + _id);
        lb.toggleClass('down');
      });
      if (lb.is(":hidden")){ // if it's removed, quit updates.
        lb.toggleClass("down");
        return;
      }
    }, 50);
  }



  $('#add_expression_button').button().click(function(){
    expdialog.dialog("open");
  });

 
 /* $('#table_name_button').button().click(function(){
    var key = $('#table_name').val();
    msg('success', key + " is selected. (prev:"+$('#mytable').find("tr:first").attr("id")+")");
    $('#mytable').find("tr").remove()
    var strRow ="<tr data-depth=1 class=\"expand\" id =\""+key+"\" key = \""+key+"\"><td>";
    strRow += "&nbsp;&nbsp;";
    strRow +="<span class=\"toggle expand\"></span>";
    strRow += "<input type = \"checkbox\" keytype = \"" + keyType + "\" id =\"checkbox_"+key;
    if (keyType == "number")
      strRow +="["+key+"]";
    else
      strRow +="."+key;
    strRow +="\">"
    strRow += key;
    strRow += "</td></tr>\n";
    $('#mytable').append(strRow);
  });
*/
  function addChildren(tr, expandAll){
    var _depth = tr.data('depth');
    var children = findChildren(tr);
    if (children.length == 0 && tr.attr('id') != "_G"){
      updateNewElements(tr,expandAll);
    }
    //Remove already collapsed nodes from children so that we don't
    //make them visible.
    //(Confused? Remove this code and close Item 2, close Item 1
    //then open Item 1 again, then you will understand)
    if (expandAll) {
      tr.removeClass('expand').addClass('collapse');
      children = findChildren(tr);
      children.show();
      children.each(function(){
        var _tr = $(this);
        if(_tr.attr("type") == "table" || _tr.attr("type") == "region" ){
          addChildren(_tr, true);
        }
      });
    }
    else
    {
      var subnodes = children.filter('.expand');
      subnodes.each(function () {
        var subnode = $(this);
        var subnodeChildren = findChildren(subnode);
        children = children.not(subnodeChildren);
      });
      //Change icon and hide/show children
      if (tr.hasClass('collapse')) {
        tr.removeClass('collapse').addClass('expand');
        children.hide();
      } else {
        tr.removeClass('expand').addClass('collapse');
        children.show();
      }
    }
    return children;
  }

 /* $('#table_cleanAll_button').button({ disabled: false }).click(function(){
  });
  */
  $('#share_button').button({ disabled: true }).click(function(){
    var at_least_one_checked = false;
    $("tr.expression").each(function(){
      var tr = $(this);
      var checked = tr.find("input[type=checkbox]").is(":checked");
      
      if (tr.attr("namespace").indexOf("__"+screen_name) != 0){
        alert("You can only share expresssions from your namespace.");
        tr.find("input[type=checkbox]").prop("checked", false);
        return;
      }

      if (checked)
        tr.remove();
      else
        return;

      at_least_one_checked = true;
      var key = tr.attr("key");
      var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, "__" + screen_name)
      ajax_run_code("__urMus_chat_post_message(\""+screen_name+"\", \" shared an expression "+key+".\",\"share_expression\",\"" +  key + "\")");
    });
    $("tr.element").each(function() {
      var tr = $(this);
      var checked = tr.find("input[type=checkbox]").is(":checked");
      var key = tr.attr("key");
      var depth = parseInt(tr.data('depth'));
      var keyType = tr.attr("keyType");
      if ( checked == false)
        return;
      if (depth != 2){
        alert("You can only share variables in top level.");
        tr.find("input[type=checkbox]").prop("checked", false);

        return;
      }
      if (tr.attr("id").indexOf("__"+screen_name) != 0){
        alert("You can only share variables from your namespace.");
        tr.find("input[type=checkbox]").prop("checked", false);
        return;
      }

      at_least_one_checked = true;
      if (keyType == "string")
        quoted_key = "\"" + key + "\"";

      ajax_run_code('__urMus_share_variable(__'+screen_name+','+quoted_key + ')')
      .done(function(){
        tr.remove();
        msg('success', "The variable <i>" + key + "</i> is shared.");
        ajax_run_code("__urMus_chat_post_message(\""+screen_name+"\", \" shared a variable "+key+".\",\"share\")");

      });
      

    });
    if(at_least_one_checked){
      updateVariableList(true, "_G");
    }
  });

  $('#table_clean_button').button().click(function(){
    var at_least_one_checked = false;
    $("tr.expression").each(function(){
      var tr = $(this);
      var checked = tr.find("input[type=checkbox]").is(":checked");
      if (checked)
        tr.remove();
      at_least_one_checked = true;
    });
    
    $("tr.element").each(function() {

      var tr = $(this);
      var checked = tr.find("input[type=checkbox]").is(":checked");
      var _id = tr.attr("id");
      if ( checked == false)
        return;
      at_least_one_checked = true;

      if(tr.attr("type") == "table"||tr.attr("type") == "region")
      {
        var children = findChildren(tr);
        children.each(function(){
          var _tr =$(this);
          _tr.find("input[type=checkbox]").prop("checked", false);
        });
        $.ajax({
          async: false,
          type: 'GET',
          url: '/eval',
          data: {
            code: '__nullify(' + _id + ')'
          },
          success:  function(data, status, xhr)  {
            ajax_run_code("__urMus_chat_post_message(\""+screen_name+"\", \" submitted code.\",\"code\")");
            msg('success', "nullified " + _id);
          },
          error: function (xhr, opts, errorThrown) {
            msg('error', "Error1:" + xhr.responseText + ":" + errorThrown);
            return;
          }
        });
        children.remove();
        children.each(function(){
          _tr = $(this);
          if ( _tr.find('a.livebutton').hasClass("down"))
          _tr.find('a.livebutton').removeClass("down");
        });
      }

      $.ajax({
        async: false,
        type: 'GET',
        url: '/eval',
        data: {
          code:  _id + ' = nil'
        },
        success:  function(data, status, xhr)  {
          msg('success', "nullified " + _id);
        },
        error: function (xhr, opts, errorThrown) {
          msg('error', "Error2:" + xhr.responseText + ":" + errorThrown);
          return;
        }
      });
      if ( tr.find('a.livebutton').hasClass("down"))
        tr.find('a.livebutton').removeClass("down");

      tr.remove();
    });
    if (!at_least_one_checked)
      alert("Check variables to be nullified.");
    else
      updateVariableList(true,"_G");
  });

  $( 'input[type=checkbox]' ).live("change", function(){
    var cb = $(this)
    var tr = cb.parent().parent(); //Get <tr> parent of toggle button
    var _checked = cb.is(":checked");

    if (_checked == false){
      var temp_id = cb.attr("id");
      temp_id = temp_id.substr(9,temp_id.length);//trim prefix checkbox_
      var _id = temp_id;
      var str = "";
      var dotpos, parpos, subpos;
      subpos = 1;
      while (true){
        dotpos = temp_id.lastIndexOf(".");
        parpos = temp_id.lastIndexOf("[");
        subpos = (dotpos>parpos?dotpos:parpos);
        if (subpos<0)
          break;
        temp_id = temp_id.substr(0,subpos);
        var escape_temp_id = temp_id.replace(/\./g,"\\.").replace(/\[/g,"\\[").replace(/\]/g,"\\]");
        str +=  "#checkbox_" + escape_temp_id+ "(" + $('#checkbox_' +escape_temp_id ).attr("keytype") + ")" + "/";
        $('#checkbox_' + escape_temp_id).prop('checked', false);
      }
    }
    if (tr.attr("type") == "table"||tr.attr("type") == "region")
      addChildren(tr, true);
    var children = findChildren(tr);
    if (children.length > 0)
    {
      children.each(function() {
        var _tr = $(this);
        var checkbox = _tr.find("input[type=checkbox]");
        checkbox.prop('checked', _checked);
      });
    }
  }); // end of input type checkbox clicked

  

  $('#open_file').button().click(function () {
    var fl = $("#file_list").slideDown();
    $('#gallery_list').slideUp()
    files.html('<h3>Files on device:</h3>');

    $(['root', 'doc']).each(function (i, dirtype) {
      $.post('/get_files', {
        dirtype: dirtype
      }, function (json) {
        files.append('<p class="header">' + ucwords(dirtype) + ':</p>');
        $.each(json, function (i, ele) {
          // for now, only do lua files
          if (ele.match(/\.lua$/)) {
              var li = $('<li class="file">' + ele + '</li>').click(function () {
              var fname = $(this).html();
              $.get('/open_file', {
                file: fname,
                dirtype: dirtype
              }, function (script) {
                msg('success', "Loaded " + fname);
                fl.slideUp(function () {
                  tabTitle.val(fname);
                  addTab();                                                                                                                                                             
                  cm[cm.length - 1].setValue(script);
                  cm[cm.length - 1].refresh();
                  cm[cm.length - 1].focus();
                });
              });
            });
          /*var li = $(' <ul class="hover"><li class="hoverli" id =">' + ele + '"> ' + ele + '<ul class="file_menu"><li><a href="#open">Open</a></li><li><a href="#rename">Rename</a></li><li><a href="#delete">Delete</a></li></ul></li></ul>');
            $("#"+ele).hover(
              function () {
                 $('#'+ele+'.file_menu').slideDown('medium');
              }, 
              function () {
                 $('#'+ele+'.file_menu').slideUp('medium');
              }
            );
*/
            files.append(li);
           // tabs.tabs('select', -1);
            
            }
        });
      }, 'json');
    });
  });  

  function playSnd(url) {
    stopSnd();
    player.jPlayer('setFile', url).jPlayer('play');
  }

  function stopSnd() {
    player.jPlayer('stop');
  }


  $('#open_gallery').button().click(function () {
    var gl = $('#gallery_list').slideDown();
    $("#file_list").slideUp()
    gallery_files.html('<h3>Resources on device:</h3>');
    // don't try to do doc at this point b/c although we can do directory
    // listing on it, it's a PIA to actual let the browser download them.
    $(['root', 'doc']).each(function (i, dirtype) {
      $.post('/get_files', {
        dirtype: dirtype
      }, function (json) {
        gallery_files.append('<p class="header">' + ucwords(dirtype) + ':</p>');
        $.each(json, function (i, ele) {
          var path = '/' + ele,
          m_img = "bmp|png|gif|jpg|jpeg|mov|mp4",
          m_snd = "mp3|wav|aif|aiff",
          li = $('<li class="resource">' + ele + '</li>');
          if (ele.match('\.(' + m_img + ')$')) {
            li.bind({
              click: function () {
                window.location = pathselect(dirtype) + path;
               // window.open(pathselect(dirtype) + path,"_blank");
              }
            });
            gallery_files.append(li);
          } else if (ele.match('\.(' + m_snd + ')$')) {
            li.bind({
              mouseover: function () {
                playSnd(path);
              },
              mouseout: function () {
                stopSnd();
              },
              click: function () {
                window.location = path;
              }
            });
            gallery_files.append(li);
           // tabs.tabs('select', -1);

          }
        });
      }, 'json');
    });
  });
  
  function msg(type, msg) {
    s.attr('class', type).html(msg);
  }
            
                
  $('#save_file').button().click(function () {
    selected = tabIndexMap[tabs.tabs('option', 'selected')-1] // => 0
    opened_file = tabLabels[selected];
    msg('success', tabs.tabs('option', 'selected') + " is selected." + selected + ":" + opened_file);

    $.ajax({
      async : false,
      type: 'POST',
      url: '/upload_script',
      data: {
        file: opened_file,
        contents: cm[selected].getValue()
      },
      success: function () {
        msg('success', "Uploaded " + opened_file);
        saved[selected] = true;
      },
      error: function () {
        msg('error', "Couldn't upload " + opened_file);
      }
    });
  });

  $('#reindent').button().click(function () {
    selected = tabIndexMap[tabs.tabs('option', 'selected')-1] // => 0
        msg('success', tabs.tabs('option', 'selected') + " is selected." + selected );

    for (i = 0; i < cm[selected].lineCount(); i++) {
      cm[selected].indentLine(i);
    }
  });

  $('#run_file').button().click(function () {
    selected = tabIndexMap[tabs.tabs('option', 'selected')-1] // => 0
    opened_file = tabLabels[selected];
    var target_namespace = namespace_setfenv.replace(/#\{namespace\}/g, "__" + screen_name);
    
    $.ajax({
      async: false,
      type: 'POST',
      url: '/eval',
      data: {
      code: target_namespace + cm[selected].getValue()
      },
      success: function (data,status, xhr) {
        msg('success', "Ran " + opened_file + ":" + xhr.responseText);
        updateVariableList(true,"__"+screen_name);
      },
      error: function (xhr, opts) {
        msg('error', xhr.responseText);
      }
    });

  });

  $('#open_log').button().click(function () {
    window.open("/html/log.html", "urMus Log", "width=400,height=600,scrollbars=1,resizable=1,location=0");
  });

  new AjaxUpload('file_upload', {
    async : false,
    action: '/upload_file',
    name: 'file',
    autoSubmit: true,
    onComplete: function (file) {
      msg('success', 'Uploaded ' + file + '!');
    }
  });
}); // END OF DOCUMENT READY FUNCTION 