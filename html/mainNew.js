function ucwords(str) {
    return (str + '').replace(/^(.)|\s(.)/g, function ($1) {
                              return $1.toUpperCase();
                              });
}

function pathselect(dirtype) {
    if (dirtype == "root") return "";
    else return "/Documents";
}
$(document).ready(function () {
                  var cm = [];
                  var tabLabels = [];
                  var tabIndexMap = [];
                  //jQuery.makeArray();
                  liveTabCounter = 0;
                  
                  $("#tabs").tabs();
                  var tabs = $("#tabs").tabs();
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
                  /*
                   tabs.find(".ui-tabs-nav").sortable({
                   axis: "x",
                   stop: function () {
                   tabs.tabs("refresh");
                   }
                   });
                   
                   */
                  // modal dialog init: custom buttons and a "close" callback reseting the form inside
                  var dialog = $("#dialog").dialog({
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
                  
                  // addTab form: calls addTab function on submit and closes the dialog
                  var form = dialog.find("form").submit(function (event) {
                                                        addTab();
                                                        dialog.dialog("close");
                                                        event.preventDefault();
                                                        
                                                        });
                  
                  var tabTitle = $("#tab_title"),
                  tabTemplate = "<li><a href='#{href}' id = '#{id}'>#{label}</a> <span class='ui-icon-close'><img src=./images/close.gif valign = middle></span></li>",
                  tabCounter = 0;
                  
                  
                  // actual addTab function: adds new tab using the input from the form above
                  function addTab() {
                  var label = tabTitle.val() || "Tab" + tabCounter + ".lua";
                  id = tabCounter;
                  li = $(tabTemplate.replace(/#\{href\}/g, "#" + id).replace(/#\{label\}/g, label).replace(/#\{id\}/g, "tab_"+id));
                  tabs.find(".ui-tabs-nav").append(li);
                  tabs.append("<div id='" + id + "'><textarea id='code" + tabCounter + "' name = 'code" + tabCounter + "'></textarea></div>");
                  tabs.tabs("refresh");
                  tabIndexMap.push(tabCounter);
                  tabLabels.push(label);
                  addEditor();
                  tabs.tabs('select', tabIndexMap.length - 1);
                  
                  cm[cm.length - 1].refresh();
                  cm[cm.length - 1].focus();
                  
                  }
                  // addTab button: just opens the dialog
                  $("#add_tab")
                  .button()
                  .click(function () {
                         tabTitle.val("");
                         
                         dialog.dialog("open");
                         });
                  
                  // close icon: removing the tab on click
                  $("#tabs span.ui-icon-close").live("click", function () {
                                                     if (confirm('Are you sure you want to delete this tab?')) {
                                                     
                                                     var panelId = $(this).closest("li").remove().attr("aria-controls");
                                                     
                                                     for (i=0; i<tabIndexMap.length; i++)
                                                     {
                                                     if(tabIndexMap[i]==parseInt(panelId))
                                                     {
                                                     cm[tabIndexMap[i]] = null;
                                                     tabLabels[tabIndexMap[i]] = null;
                                                     tabIndexMap.splice(i,1);
                                                     break;
                                                     }
                                                     }
                                                     
                                                     
                                                     selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                                                     
                                                     $("#" + panelId).remove();
                                                     tabs.tabs("refresh");
                                                     }
                                                     });
                  
                  function addEditor() {
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
                                                           "Cmd-R": function(instance) {                                                     selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                                                           opened_file = tabLabels[selected];
                                                           $.ajax({
                                                                  type: 'POST',
                                                                  url: '/eval',
                                                                  data: {
                                                                  code: cm[selected].getSelection()
                                                                  },
                                                                  success: function () {
                                                                  msg('success', "Ran " + opened_file);
                                                                  },
                                                                  error: function (xhr, opts) {
                                                                  msg('error', xhr.responseText);
                                                                  }
                                                                  });
                                                           
                                                           
                                                           
                                                           },
                                                           "Ctrl-R": function(instance) {                                                     selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                                                           opened_file = tabLabels[selected];
                                                           $.ajax({
                                                                  type: 'POST',
                                                                  url: '/eval',
                                                                  data: {
                                                                  code: cm[selected].getSelection()
                                                                  },
                                                                  success: function () {
                                                                  msg('success', "Ran " + opened_file);
                                                                  },
                                                                  error: function (xhr, opts) {
                                                                  msg('error', xhr.responseText);
                                                                  }
                                                                  });
                                                           
                                                           
                                                           
                                                           }
                                                           }
                                                           });
                  cm.push(tempEditor);
                  tabCounter++;
                  
                  
                  }
                  
                  //addEditor();
                  
                  
                  $('#open_file').button().click(function () {
                                                 var fl = $("#file_list").slideDown();
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
                                                                                                                                                              addTab();                                                                                                                                                              cm[cm.length - 1].setValue(script);
                                                                                                                                                              cm[cm.length - 1].refresh();
                                                                                                                                                              cm[cm.length - 1].focus();
                                                                                                                                                              
                                                                                                                                                              });
                                                                                                                                                   // setOpenedFile(fname);
                                                                                                                                                   });
                                                                                                                                             });
                                                                                       files.append(li);
                                                                                       }
                                                                                       });
                                                                                }, 'json');
                                                                         });
                                                 });
                  
                  $('#reindent').button()
                  .click(function () {
                         
                         selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                         for (i = 0; i < cm[selected].lineCount(); i++) {
                         cm[selected].indentLine(i);
                         }
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
                                                                                          
                                                                                          //            li.qtip({content:'<img src="'+path+'"/>'});
                                                                                          li.bind({
                                                                                                  click: function () {
                                                                                                  window.location = pathselect(dirtype) + path;
                                                                                                  }
                                                                                                  //					   var fname = $(this).html();
                                                                                                  //					   $.get('/open_media',{file:fname, dirtype:dirtype}, function(script) {script;}); }
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
                                                                                          }
                                                                                          
                                                                                          });
                                                                                   }, 'json');
                                                                            });
                                                    
                                                    //   $('#file_list1').fileTree({
                                                    //							   root: '/',
                                                    //							   script: 'jqueryFileTree',
                                                    //							   expandSpeed: 1000,
                                                    //							   collapseSpeed: 1000,
                                                    //							   multiFolder: false
                                                    //							   }, function(file) {
                                                    //							   alert(file);
                                                    //							   });
                                                    
                                                    });
                  /*
                   
                   function setOpenedFile(fname) {
                   opened_file = fname;
                   $('#save_file').removeAttr('disabled');
                   }
                   */
                  
                  function msg(type, msg) {
                  s.attr('class', type).html(msg);
                  }
                  
                  // on submission of inline text, eval code
                  $('#small_code_button').button().click(function () {
                                                         $.ajax({
                                                                type: 'POST',
                                                                url: '/eval',
                                                                data: {
                                                                code: $('#small_code').val()
                                                                },
                                                                success: function () {
                                                                msg('success', "Success!");
                                                                },
                                                                error: function (xhr, opts) {
                                                                msg('error', xhr.responseText);
                                                                }
                                                                });
                                                         
                                                         e.preventDefault();
                                                         return false;
                                                         });
                  
                  $('#save_file').button().click(function () {
                                                 
                                                 selected = tabIndexMap[tabs.tabs('option', 'selected')] // => 0
                                                 opened_file = tabLabels[selected];
                                                 
                                                 
                                                 $.ajax({
                                                        type: 'POST',
                                                        url: '/upload_script',
                                                        data: {
                                                        file: opened_file,
                                                        contents: cm[selected].getValue()
                                                        },
                                                        success: function () {
                                                        msg('success', "Uploaded " + opened_file);
                                                        },
                                                        error: function () {
                                                        msg('error', "Couldn't upload " + opened_file);
                                                        }
                                                        });
                                                 });
                  $('#reindent').button()
                  .click(function () {
                         selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                         for (i = 0; i < cm[selected].lineCount(); i++) {
                         cm[selected].indentLine(i);
                         }
                         });
                  
                  
                  
                  $('#run_file').button().click(function () {
                                                selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                                                opened_file = tabLabels[selected];
                                                $.ajax({
                                                       type: 'POST',
                                                       url: '/eval',
                                                       data: {
                                                       code: cm[selected].getValue()
                                                       },
                                                       success: function () {
                                                       msg('success', "Ran " + opened_file);
                                                       },
                                                       error: function (xhr, opts) {
                                                       msg('error', xhr.responseText);
                                                       }
                                                       });
                                                });
                  
                  $('#extend_box').button().click(function () {
                                                  var box = $(".CodeMirror-wrapping")
                                                  box[0].style.height = (parseInt(box[0].style.height) + 100) + "px";
                                                  });
                  $('#selection').button().click(function () {
                                                 selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                                                 opened_file = tabLabels[selected];
                                                 $.ajax({
                                                        type: 'POST',
                                                        url: '/eval',
                                                        data: {
                                                        code: cm[selected].getSelection()
                                                        },
                                                        success: function () {
                                                        msg('success', "Ran " + opened_file);
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
                                 action: '/upload_file',
                                 name: 'file',
                                 autoSubmit: true,
                                 onComplete: function (file) {
                                 msg('success', 'Uploaded ' + file + '!');
                                 }
                                 });
                  
                  /*      $.get('/open_file', {
                   file: "urBlank.lua",
                   dirtype: 'root'
                   }, function (script) {
                   selected = tabIndexMap[tabs.tabs('option', 'selected')]; // => 0
                   msg('success', "Loaded " + "urBlank.lua");
                   cm[selected].setValue(script);
                   setOpenedFile("urBlank.lua");
                   });
                   
                   */
                  });
