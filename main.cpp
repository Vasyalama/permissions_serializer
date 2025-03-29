#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Buffer.H>

#include <string> 
#include <filesystem>
#include "kserialize.h"

namespace fs = std::filesystem;

Fl_Input* input = nullptr;
Fl_Input* output = nullptr;
Fl_Button* action_button = nullptr;
Fl_Choice* input_mode = nullptr;
Fl_Choice* output_mode = nullptr;
Fl_Check_Button* serialize_btn = nullptr;
Fl_Check_Button* deserialize_btn = nullptr;
Fl_Text_Buffer* log_buffer = nullptr;
Fl_Text_Editor* log_editor = nullptr;

void addToLog(std::u8string message);

void throw_u8string_error(std::u8string s) {
    throw std::runtime_error(std::string(reinterpret_cast<const char*>(&s[0])));
}

void mode_callback(Fl_Widget* w, void* data) {
    Fl_Check_Button* b = (Fl_Check_Button*)w;
    if (b->value() && action_button ) {
        action_button ->label(b->label());
    }
}

void choose_path(Fl_Input* target, bool want_directory) {
    Fl_Native_File_Chooser chooser;
    chooser.title(want_directory ? "Select Directory" : "Select File");
    chooser.type(want_directory ? Fl_Native_File_Chooser::BROWSE_DIRECTORY
        : Fl_Native_File_Chooser::BROWSE_FILE);

    if (chooser.show() == 0) {
        target->value(chooser.filename());
    }
}

void browse_callback(Fl_Widget* w, void* data) {
    bool is_input = (w->user_data() == (void*)1);
    Fl_Choice* mode_choice = is_input ? input_mode : output_mode;
    Fl_Input* path_input = is_input ? input : output;

    bool want_directory = (mode_choice->value() == 1);
    choose_path(path_input, want_directory);
}

void action_callback(Fl_Widget* w, void*) {
    try {
        fs::path input_path = fs::path(fs::u8path(input->value()).native());
        fs::path output_path = fs::path(fs::u8path(output->value()).native());

        if (serialize_btn && serialize_btn->value()) {
            if (input_path.native().empty()) {
                fl_alert("input path is empty. please select file or folder to serialize");
                return;
            }

            if (output_path.native().empty()) {
                output_path = input_path.parent_path();
            }

            fs::path kser_file_path;
            if (fs::is_directory(output_path)) {
                kser_file_path = output_path / input_path.filename();
                kser_file_path.replace_extension(".kser");

                if (fs::exists(kser_file_path)) {
                    if (!fl_ask("you provided an output folder path that already contains a file named %s. do you want to \
                            overwrite it?\n\n HINT: if you want to use that file for extracting permissions for  other \
                            systems please select output type as file and choose the correct file. ")) {
                        return;
                    }
                    else {
                        if (!fs::remove(kser_file_path)) {
                            throw_u8string_error(u8"error deleting " + kser_file_path.u8string());
                        }
                    }
                }

                std::ofstream outfile(kser_file_path);
                if (!outfile) {
                    throw_u8string_error(u8"failed to create serialized file: " + kser_file_path.u8string());
                }
                outfile.close();
                addToLog(u8"Created file " + kser_file_path.u8string());
            }
            else {
                if (output_path.extension() != ".kser") {
                    fl_alert("output file must have .kser extension");
                    return;
                }
                kser_file_path = output_path;
            }
            serialize(input_path, kser_file_path);
            addToLog(u8"finished serializing");
        }
        else if (deserialize_btn && deserialize_btn->value()) {
            if (input_path.native().empty()) {
                fl_alert("input file path is empty. please provide a .kser input file");
                return;
            }

            if (fs::is_directory(input_path)) {
                fl_alert("input must be a .kser file, not a directory");
                return;
            }

            if (input_path.extension() != ".kser") {
                fl_alert("input file must have .kser extension");
                return;
            }

            if (fs::is_empty(input_path)) {
                fl_alert("input file is empty. nothing to deserialize");
                return;
            }

            
            if (output_path.native().empty()) {
                output_path = input_path.parent_path();
            }
            deserialize(input_path, output_path);
            
            addToLog(u8"finished deserializing");
        }
        else {
            //unreachable
            throw_u8string_error(u8"unreachable runtime error");
        }
    }
    catch (std::exception& e) {
        fl_alert("ERROR: %s", e.what());
    }
}

int main(int argc, char** argv) {
    Fl_Window* window = new Fl_Window(800, 600, "Permissions Zipper");

    Fl_Group* mode_group = new Fl_Group(20, 20, 560, 40);
    serialize_btn = new Fl_Check_Button(20, 20, 100, 30, "Serialize");
    deserialize_btn = new Fl_Check_Button(140, 20, 100, 30, "Deserialize");

    serialize_btn->type(FL_RADIO_BUTTON);
    deserialize_btn->type(FL_RADIO_BUTTON);
    serialize_btn->setonly();
    serialize_btn->callback(mode_callback);
    deserialize_btn->callback(mode_callback);
    mode_group->end();

    Fl_Group* input_group = new Fl_Group(20, 80, 560, 80);
    new Fl_Box(20, 80, 100, 20, "Input:");

    input_mode = new Fl_Choice(20, 100, 100, 30, "Type:");
    input_mode->add("File");
    input_mode->add("Folder");
    input_mode->value(0);

    Fl_Button* input_browse = new Fl_Button(130, 100, 80, 30, "Browse");
    input_browse->user_data((void*)1);
    input = new Fl_Input(220, 100, 360, 30);
    input_group->end();

    Fl_Group* output_group = new Fl_Group(20, 180, 560, 80);
    new Fl_Box(20, 180, 100, 20, "Output:");

    output_mode = new Fl_Choice(20, 200, 100, 30, "Type:");
    output_mode->add("File");
    output_mode->add("Folder");
    output_mode->value(0);

    Fl_Button* output_browse = new Fl_Button(130, 200, 80, 30, "Browse");
    output_browse->user_data((void*)0);
    output = new Fl_Input(220, 200, 360, 30);
    output_group->end();

    
    action_button = new Fl_Button(250, 290, 100, 40, "Serialize");
    action_button ->labelfont(FL_BOLD);
    action_button ->labelsize(16);

    input_browse->callback(browse_callback);
    output_browse->callback(browse_callback);
    action_button ->callback(action_callback);


    log_buffer = new Fl_Text_Buffer();
    log_editor = new Fl_Text_Editor(20, 350, 760, 200, "Log");
    log_editor->buffer(log_buffer);
    log_editor->textfont(FL_COURIER);
    log_editor->scrollbar_width(15);

    window->end();
    window->show(argc, argv);
    return Fl::run();
}

void addToLog(std::u8string message) {
    message += '\n\0';
    if (log_buffer && log_editor) {
        log_buffer->append(reinterpret_cast<const char*>(&message[0]));
        log_editor->insert_position(log_buffer->length());
        log_editor->show_insert_position();
        Fl::check();
    }
}