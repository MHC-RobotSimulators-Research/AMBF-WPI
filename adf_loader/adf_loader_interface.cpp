//==============================================================================
/*
    Software License Agreement (BSD License)
    Copyright (c) 2020, AMBF
    (https://github.com/WPI-AIM/ambf)

    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

    * Neither the name of authors nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
    ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    \author    <amunawar@wpi.edu>
    \author    Adnan Munawar
    \version   1.0$
*/
//==============================================================================

//------------------------------------------------------------------------------
#include <adf_loader_interface.h>

using namespace std;
using namespace adf_loader_1_0;

///
/// \brief afConfigHandler::load_yaml
/// \param a_config_file
/// \return
///
bool ADFLoaderInterface::loadLaunchFile(string a_filepath, afLaunchAttributes* attribs){

}


///
/// \brief afConfigHandler::get_puzzle_config
/// \return
///
string ADFLoaderInterface::getMultiBodyFilepath(uint i){
    if (i <= getNumMBFilepaths()){
        return m_multiBodyFilepaths[i];
    }
    else{
        //printf("i = %d, Whereas only %d multi bodies specified", i, s_multiBodyConfigFileNames.size());
        printf("i = %d, Whereas only %lu multi bodies specified", i, (unsigned long)m_multiBodyFilepaths.size());
        return "";
    }
}


///
/// \brief afConfigHandler::get_color_rgba
/// \param a_color_name
/// \return
///
adfVersion ADFLoaderInterface::getFileVersion(string a_filepath)
{

}

adfVersion ADFLoaderInterface::getFileVersion(YAML::Node *a_node)
{
    assert(a_node == nullptr);

    YAML::Node &node = *a_node;

    YAML::Node versionNode = node["adf version"];

    if (versionNode.IsDefined()){
        std::string versionStr = versionNode.as<string>();

        return getVersionFromString(versionStr);
    }
    else{
        cerr << "Version not defined thus assuming VERSION_1_0";
        return adfVersion::VERSION_1_0;
    }
}

bool ADFLoaderInterface::setLoaderVersion(adfVersion a_version){
    switch (a_version) {
    case adfVersion::VERSION_1_0:{
        if (getVersionFromString(getLoaderVersion()) == a_version){
            // Have already loaded the right version. Ignore
            break;
        }
        else{
            ADFLoader_1_0 loader = new ADFLoader_1_0();
            setLoader(loader);
        }
        break;
    }
//    case afLoaderVersion::VERSION_2_0:{
//        if (getVersionFromString(getVersion()) == a_version){
//            // Have already loaded the right version. Ignore
//            break;
//        }
//        else{
//            ADFLoader_2_0 loader = new ADFLoader_2_0();
//            setLoader(loader);
//        }
//        break;
//    }
    default:
        break;
    }

    return true;
}

adfVersion ADFLoaderInterface::getVersionFromString(string a_str){
    if (a_str.compare("1.0") == 0){
        return adfVersion::VERSION_1_0;
    }
    else{
        return adfVersion::INVALID;
    }
}

vector<double> ADFLoaderInterface::getColorRGBA(string a_color_name){
    vector<double> rgba = {0.5, 0.5, 0.5, 1.0};
    // Help from https://stackoverflow.com/questions/15425442/retrieve-random-key-element-for-stdmap-in-c
    if(strcmp(a_color_name.c_str(), "random") == 0 || strcmp(a_color_name.c_str(), "RANDOM") == 0){
        YAML::const_iterator it = m_colorsNode.begin();
        advance(it, rand() % m_colorsNode.size());
        rgba[0] = it->second["r"].as<int>() / 255.0;
        rgba[1] = it->second["g"].as<int>() / 255.0;
        rgba[2] = it->second["b"].as<int>() / 255.0;
        rgba[3] = it->second["a"].as<int>() / 255.0;
    }
    else if(m_colorsNode[a_color_name].IsDefined()){
        rgba[0] = m_colorsNode[a_color_name]["r"].as<int>() / 255.0;
        rgba[1] = m_colorsNode[a_color_name]["g"].as<int>() / 255.0;
        rgba[2] = m_colorsNode[a_color_name]["b"].as<int>() / 255.0;
        rgba[3] = m_colorsNode[a_color_name]["a"].as<int>() / 255.0;
    }
    else{
        cerr << "WARNING! COLOR NOT FOUND, RETURNING DEFAULT COLOR\n";
    }
    return rgba;
}
