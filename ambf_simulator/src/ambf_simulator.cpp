//===========================================================================
/*
    Software License Agreement (BSD License)
    Copyright (c) 2019, AMBF
    (www.aimlab.wpi.edu)

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

    \author:    <http://www.aimlab.wpi.edu>
    \author:    <amunawar@wpi.edu>
    \author:    Adnan Munawar
    \courtesy:  Starting point CHAI3D-BULLET examples by Francois Conti from <www.chai3d.org>
    \version:   $
*/
//===========================================================================

//---------------------------------------------------------------------------
#include "chai3d.h"
#include "ambf.h"
//---------------------------------------------------------------------------
#include <GLFW/glfw3.h>
#include <boost/program_options.hpp>
#include <mutex>
//---------------------------------------------------------------------------
using namespace ambf;
using namespace chai3d;
using namespace std;
//---------------------------------------------------------------------------
#include "CBullet.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// GENERAL SETTINGS
//---------------------------------------------------------------------------

// stereo Mode
/*
    C_STEREO_DISABLED:            Stereo is disabled
    C_STEREO_ACTIVE:              Active stereo for OpenGL NVDIA QUADRO cards
    C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
    C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// fullscreen mode
bool fullscreen = false;

// mirrored display
bool mirroredDisplay = false;


//---------------------------------------------------------------------------
// BULLET MODULE VARIABLES
//---------------------------------------------------------------------------

// bullet world
cBulletWorld* g_bulletWorld;

// bullet static walls and ground
cBulletStaticPlane* g_bulletGround;

cBulletStaticPlane* g_bulletBoxWallX[2];
cBulletStaticPlane* g_bulletBoxWallY[2];
cBulletStaticPlane* g_bulletBoxWallZ[1];

afMultiBody *g_afMultiBody;
afWorld *g_afWorld;

cVector3d g_camPos(0,0,0);
cVector3d g_dev_vel;
double g_dt_fixed = 0;
bool g_force_enable = true;
// Default switch index for clutches

// a label to display the rates [Hz] at which the simulation is running
cLabel* g_labelRates;
cLabel* g_labelTimes;
cLabel* g_labelModes;
cLabel* g_labelBtnAction;
std::string g_btn_action_str = "";
bool g_cam_btn_pressed = false;
bool g_clutch_btn_pressed = false;
cPrecisionClock g_clockWorld;


//---------------------------------------------------------------------------
// GENERAL VARIABLES
//---------------------------------------------------------------------------

// flag to indicate if the haptic simulation currently running
bool g_simulationRunning = false;

// flag to indicate if the haptic simulation has terminated
bool g_simulationFinished = true;

// a frequency counter to measure the simulation graphic rate
cFrequencyCounter g_freqCounterGraphics;

// a frequency counter to measure the simulation haptic rate
cFrequencyCounter g_freqCounterHaptics;

// haptic thread
std::vector<cThread*> g_hapticsThreads;
// bullet simulation thread
cThread* g_bulletSimThread;

// current width of window
int g_width = 0;

// current height of window
int g_height = 0;

// swap interval for the display context (vertical synchronization)
int g_swapInterval = 1;

//---------------------------------------------------------------------------
// DECLARED MACROS
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// DECLARED FUNCTIONS
//---------------------------------------------------------------------------

// callback when the window display is resized
void windowSizeCallback(GLFWwindow* a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void errorCallback(int error, const char* a_description);

// callback when a key is pressed
void keyCallback(GLFWwindow* a_window, int a_key, int a_scancode, int a_action, int a_mods);

// this function contains the main haptics simulation loop
void updateHaptics(void*);

//this function contains the main Bullet Simulation loop
void updateBulletSim(void);

// this function closes the application
void close(void);

const int MAX_DEVICES = 10;

//Forward Declarations
class PhysicalDevice;
class SimulatedGripper;
struct DeviceGripperPair;

std::vector<cLabel*> m_labelDevRates;

///
/// \brief The VisualHandle struct
///
struct WindowCameraHandle{
  GLFWwindow* m_window = NULL;
  GLFWmonitor* m_monitor = NULL;
  cCamera* m_camera;
  std::vector<DeviceGripperPair> m_dgPair;
  std::vector<std::string> m_deviceNames;
  int m_height = 0;
  int m_width = 0;
  int m_swapInterval = 1;
  std::vector<cMatrix3d> m_cam_rot_last;
  std::vector<cMatrix3d> m_dev_rot_last;
  std::vector<cMatrix3d> m_dev_rot_cur;
  std::vector<bool> m_cam_pressed;
};

// Vector of WindowCamera Handles Struct
std::vector<WindowCameraHandle> g_windowCameraHandles;

// Global iterator for WindowsCamera Handle
std::vector<WindowCameraHandle>::iterator g_winCamIt;

// this function renders the scene
void updateGraphics(WindowCameraHandle a_winCamHandle);

///
/// \brief This class encapsulates each haptic device in isolation and provides methods to get/set device
/// state/commands, button's state and grippers state if present
///
class PhysicalDevice{
public:
    PhysicalDevice(){}
    virtual ~PhysicalDevice();
    virtual cVector3d measured_pos();
    virtual cMatrix3d measured_rot();
    virtual cVector3d measured_lin_vel();
    virtual cVector3d mearured_ang_vel();
    virtual double measured_gripper_angle();
    virtual void apply_wrench(cVector3d a_force, cVector3d a_torque);
    virtual bool is_button_pressed(int button_index);
    virtual bool is_button_press_rising_edge(int button_index);
    virtual bool is_button_press_falling_edge(int button_index);
    void enable_force_feedback(bool enable){m_dev_force_enabled = enable;}
    cShapeSphere* create_cursor(cBulletWorld* a_world);
    cBulletSphere* create_af_cursor(cBulletWorld* a_world, std::string a_name);
    cGenericHapticDevicePtr m_hDevice;
    cHapticDeviceInfo m_hInfo;
    cVector3d m_pos, m_posClutched, m_vel, m_avel;
    cMatrix3d m_rot, m_rotClutched;
    double m_workspace_scale_factor;
    cShapeSphere* m_cursor = NULL;
    cBulletSphere* m_af_cursor = NULL;
    bool m_btn_prev_state_rising[10] = {false};
    bool m_btn_prev_state_falling[10] = {false};
    cFrequencyCounter m_freq_ctr;

private:
    std::mutex m_mutex;
    void update_cursor_pose();
    bool m_dev_force_enabled = true;
};

///
/// \brief PhysicalDevice::create_cursor
/// \param a_world
/// \return
///
cShapeSphere* PhysicalDevice::create_cursor(cBulletWorld* a_world){
    m_cursor = new cShapeSphere(0.05);
    m_cursor->setShowEnabled(true);
    m_cursor->setShowFrame(true);
    m_cursor->setFrameSize(0.1);
    cMaterial mat;
    mat.setGreenLightSea();
    m_cursor->setMaterial(mat);
    a_world->addChild(m_cursor);
    return m_cursor;
}

///
/// \brief PhysicalDevice::~PhysicalDevice
///
PhysicalDevice::~PhysicalDevice(){
}

///
/// \brief PhysicalDevice::create_af_cursor
/// \param a_world
/// \param a_name
/// \return
///
cBulletSphere* PhysicalDevice::create_af_cursor(cBulletWorld *a_world, string a_name){
    m_af_cursor = new cBulletSphere(a_world, 0.05, a_name);
    m_af_cursor->setShowEnabled(true);
    m_af_cursor->setShowFrame(true);
    m_af_cursor->setFrameSize(0.1);
    cMaterial mat;
    mat.setGreenLightSea();
    m_af_cursor->setMaterial(mat);
    a_world->addChild(m_af_cursor);
    return m_af_cursor;
}

///
/// \brief PhysicalDevice::measured_pos
/// \return
///
cVector3d PhysicalDevice::measured_pos(){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hDevice->getPosition(m_pos);
    update_cursor_pose();
    return m_pos;
}

///
/// \brief PhysicalDevice::measured_rot
/// \return
///
cMatrix3d PhysicalDevice::measured_rot(){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hDevice->getRotation(m_rot);
    return m_rot;
}

///
/// \brief PhysicalDevice::update_cursor_pose
///
void PhysicalDevice::update_cursor_pose(){
    if(m_cursor){
        m_cursor->setLocalPos(m_pos * m_workspace_scale_factor);
        m_cursor->setLocalRot(m_rot);
    }
    if(m_af_cursor){
        m_af_cursor->setLocalPos(m_pos * m_workspace_scale_factor);
        m_af_cursor->setLocalRot(m_rot);
#ifdef C_ENABLE_AMBF_COMM_SUPPORT
        m_af_cursor->m_afObjectPtr->set_userdata_desc("haptics frequency");
        m_af_cursor->m_afObjectPtr->set_userdata(m_freq_ctr.getFrequency());
#endif
    }
}

///
/// \brief PhysicalDevice::measured_lin_vel
/// \return
///
cVector3d PhysicalDevice::measured_lin_vel(){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hDevice->getLinearVelocity(m_vel);
    return m_vel;
}

///
/// \brief PhysicalDevice::mearured_ang_vel
/// \return
///
cVector3d PhysicalDevice::mearured_ang_vel(){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hDevice->getAngularVelocity(m_avel);
    return m_avel;
}

///
/// \brief PhysicalDevice::measured_gripper_angle
/// \return
///
double PhysicalDevice::measured_gripper_angle(){
    std::lock_guard<std::mutex> lock(m_mutex);
    double angle;
    m_hDevice->getGripperAngleRad(angle);
    return angle;
}

///
/// \brief PhysicalDevice::is_button_pressed
/// \param button_index
/// \return
///
bool PhysicalDevice::is_button_pressed(int button_index){
    std::lock_guard<std::mutex> lock(m_mutex);
    bool status;
    m_hDevice->getUserSwitch(button_index, status);
    return status;
}

///
/// \brief PhysicalDevice::is_button_press_rising_edge
/// \param button_index
/// \return
///
bool PhysicalDevice::is_button_press_rising_edge(int button_index){
    std::lock_guard<std::mutex> lock(m_mutex);
    bool status;
    m_hDevice->getUserSwitch(button_index, status);
    if (m_btn_prev_state_rising[button_index] ^ status){
        if (!m_btn_prev_state_rising[button_index]){
            m_btn_prev_state_rising[button_index] = true;
            return true;
        }
        else{
            m_btn_prev_state_rising[button_index] = false;
        }
    }
    return false;
}

///
/// \brief PhysicalDevice::is_button_press_falling_edge
/// \param button_index
/// \return
///
bool PhysicalDevice::is_button_press_falling_edge(int button_index){
    std::lock_guard<std::mutex> lock(m_mutex);
    bool status;
    m_hDevice->getUserSwitch(button_index, status);
    if (m_btn_prev_state_falling[button_index] ^ status){
        if (m_btn_prev_state_falling[button_index]){
            m_btn_prev_state_falling[button_index] = false;
            return true;
        }
        else{
            m_btn_prev_state_falling[button_index] = true;
        }
    }
    return false;
}

///
/// \brief PhysicalDevice::apply_wrench
/// \param force
/// \param torque
///
void PhysicalDevice::apply_wrench(cVector3d force, cVector3d torque){
    std::lock_guard<std::mutex> lock(m_mutex);
    force = force * m_dev_force_enabled;
    torque = torque * m_dev_force_enabled;
    m_hDevice->setForceAndTorqueAndGripperForce(force, torque, 0.0);
}

///
/// \brief This class encapsulates Simulation Parameters that deal with the interaction between a single haptic device
/// and the related Gripper simulated in Bullet. These Parameters include mapping the device buttons to
/// action/mode buttons, capturing button triggers in addition to presses, mapping the workspace scale factors
/// for a device and so on.
///
class SimulationParams{
public:
    SimulationParams(){
        m_workspaceScaleFactor = 30.0;
        K_lh = 0.02;
        K_ah = 0.03;
        K_lc = 200;
        K_ac = 30;
        B_lc = 5.0;
        B_ac = 3.0;
        K_lh_ramp = 0.0;
        K_ah_ramp = 0.0;
        K_lc_ramp = 0.0;
        K_ac_ramp = 0.0;
        act_1_btn   = 0;
        act_2_btn   = 1;
        mode_next_btn = 2;
        mode_prev_btn= 3;

        m_posRef.set(0,0,0);
        m_posRefOrigin.set(0, 0, 0);
        m_rotRef.identity();
        m_rotRefOrigin.identity();

        btn_cam_rising_edge = false;
        btn_clutch_rising_edge = false;
        m_loop_exec_flag = false;
    }
    void set_sim_params(cHapticDeviceInfo &a_hInfo, PhysicalDevice* a_dev);
    inline void set_loop_exec_flag(){m_loop_exec_flag=true;}
    inline void clear_loop_exec_flag(){m_loop_exec_flag = false;}
    inline bool is_loop_exec(){return m_loop_exec_flag;}
    inline double get_workspace_scale_factor(){return m_workspaceScaleFactor;}
    cVector3d m_posRef, m_posRefOrigin;
    cMatrix3d m_rotRef, m_rotRefOrigin;
    double m_workspaceScaleFactor;
    double K_lh;                    //Linear Haptic Stiffness Gain
    double K_ah;                    //Angular Haptic Stiffness Gain
    double K_lh_ramp;               //Linear Haptic Stiffness Gain Ramped
    double K_ah_ramp;               //Angular Haptic Stiffness Gain Ramped
    double K_lc_ramp;               //Linear Haptic Stiffness Gain Ramped
    double K_ac_ramp;               //Angular Haptic Stiffness Gain Ramped
    double K_lc;                    //Linear Controller Stiffness Gain
    double K_ac;                    //Angular Controller Stiffness Gain
    double B_lc;                    //Linear Controller Damping Gain
    double B_ac;                    //Angular Controller Damping Gain
    int act_1_btn;
    int act_2_btn;
    int mode_next_btn;
    int mode_prev_btn;
    int m_gripper_pinch_btn = -1;
    bool btn_cam_rising_edge;
    bool btn_clutch_rising_edge;
    bool m_loop_exec_flag;
};

///
/// \brief SimulationParams::set_sim_params
/// \param a_hInfo
/// \param a_dev
///
void SimulationParams::set_sim_params(cHapticDeviceInfo &a_hInfo, PhysicalDevice* a_dev){
    double maxStiffness	= a_hInfo.m_maxLinearStiffness / m_workspaceScaleFactor;

    // clamp the force output gain to the max device stiffness
    K_lh = cMin(K_lh, maxStiffness / K_lc);
    if (strcmp(a_hInfo.m_modelName.c_str(), "MTM-R") == 0 || strcmp(a_hInfo.m_modelName.c_str(), "MTMR") == 0 ||
            strcmp(a_hInfo.m_modelName.c_str(), "MTM-L") == 0 || strcmp(a_hInfo.m_modelName.c_str(), "MTML") == 0)
    {
        std::cout << "Device " << a_hInfo.m_modelName << " DETECTED, CHANGING BUTTON AND WORKSPACE MAPPING" << std::endl;
        m_workspaceScaleFactor = 10.0;
        K_lh = K_lh/3;
        act_1_btn     =  1;
        act_2_btn     =  2;
        mode_next_btn =  3;
        mode_prev_btn =  4;
        K_lh = 0.04;
        K_ah = 0.0;
        m_gripper_pinch_btn = 0;
        a_dev->enable_force_feedback(false);
    }

    if (strcmp(a_hInfo.m_modelName.c_str(), "Falcon") == 0)
    {
        std::cout << "Device " << a_hInfo.m_modelName << " DETECTED, CHANGING BUTTON AND WORKSPACE MAPPING" << std::endl;
        act_1_btn     = 0;
        act_2_btn     = 2;
        mode_next_btn = 3;
        mode_prev_btn = 1;
        K_lh = 0.05;
        K_ah = 0.0;
    }

    if (strcmp(a_hInfo.m_modelName.c_str(), "PHANTOM Omni") == 0)
    {
        std::cout << "Device " << a_hInfo.m_modelName << " DETECTED, CHANGING BUTTON AND WORKSPACE MAPPING" << std::endl;
        K_lh = 0.01;
        K_ah = 0.0;
    }

    if (strcmp(a_hInfo.m_modelName.c_str(), "Razer Hydra") == 0)
    {
        std::cout << "Device " << a_hInfo.m_modelName << " DETECTED, CHANGING BUTTON AND WORKSPACE MAPPING" << std::endl;
        m_workspaceScaleFactor = 10.0;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///
/// \brief The SimulatedGripper class
///
class SimulatedGripper: public SimulationParams, public afGripper{
public:
    SimulatedGripper(cBulletWorld *a_chaiWorld);
    ~SimulatedGripper(){}
    bool loadFromAMBF(std::string a_gripper_name, std::string a_device_name);
    virtual cVector3d measured_pos();
    virtual cMatrix3d measured_rot();
    virtual void update_measured_pose();
    virtual inline void apply_force(cVector3d force){if (!m_rootLink->m_af_enable_position_controller) m_rootLink->addExternalForce(force);}
    virtual inline void apply_torque(cVector3d torque){if (!m_rootLink->m_af_enable_position_controller) m_rootLink->addExternalTorque(torque);}
    bool is_wrench_set();
    void clear_wrench();
    void offset_gripper_angle(double offset);
    void setGripperAngle(double angle, double dt=0.001);
    cVector3d m_pos;
    cMatrix3d m_rot;
    double m_gripper_angle;

    std::mutex m_mutex;
};

///
/// \brief SimulatedGripper::SimulatedGripper
/// \param a_chaiWorld
///
SimulatedGripper::SimulatedGripper(cBulletWorld *a_chaiWorld): afGripper (a_chaiWorld){
    m_gripper_angle = 0.5;
}

///
/// \brief SimulatedGripper::loadFromAMBF
/// \param a_gripper_name
/// \param a_device_name
/// \return
///
bool SimulatedGripper::loadFromAMBF(std::string a_gripper_name, std::string a_device_name){
    std::string config = getGripperConfig(a_device_name);
    bool res = loadMultiBody(config, a_gripper_name, a_device_name);
    m_rootLink = getRootRigidBody();
    return res;
}

///
/// \brief SimulatedGripper::measured_pos
/// \return
///
cVector3d SimulatedGripper::measured_pos(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rootLink->getLocalPos();
}

///
/// \brief SimulatedGripper::measured_rot
/// \return
///
cMatrix3d SimulatedGripper::measured_rot(){
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rootLink->getLocalRot();
}

///
/// \brief SimulatedGripper::update_measured_pose
///
void SimulatedGripper::update_measured_pose(){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pos  = m_rootLink->getLocalPos();
    m_rot = m_rootLink->getLocalRot();
}

///
/// \brief SimulatedGripper::setGripperAngle
/// \param angle
/// \param dt
///
void SimulatedGripper::setGripperAngle(double angle, double dt){
    m_rootLink->setAngle(angle, dt);
}

///
/// \brief SimulatedGripper::offset_gripper_angle
/// \param offset
///
void SimulatedGripper::offset_gripper_angle(double offset){
    std::lock_guard<std::mutex> lock(m_mutex);
    m_gripper_angle += offset;
    m_gripper_angle = cClamp(m_gripper_angle, 0.0, 1.0);
}

///
/// \brief SimulatedGripper::is_wrench_set
/// \return
///
bool SimulatedGripper::is_wrench_set(){
    btVector3 f = m_rootLink->m_bulletRigidBody->getTotalForce();
    btVector3 n = m_rootLink->m_bulletRigidBody->getTotalTorque();
    if (f.isZero()) return false;
    else return true;
}

///
/// \brief SimulatedGripper::clear_wrench
///
void SimulatedGripper::clear_wrench(){
    m_rootLink->m_bulletRigidBody->clearForces();
}

///
/// \brief These are the currently availble modes for each device
///
enum MODES{ CAM_CLUTCH_CONTROL,
            GRIPPER_JAW_CONTROL,
            CHANGE_CONT_LIN_GAIN,
            CHANGE_CONT_ANG_GAIN,
            CHANGE_CONT_LIN_DAMP,
            CHANGE_CONT_ANG_DAMP,
            CHANGE_DEV_LIN_GAIN,
            CHANGE_DEV_ANG_GAIN
          };


///
/// \brief The DeviceGripperPair struct
///
struct DeviceGripperPair{
    PhysicalDevice* m_physicalDevice;
    SimulatedGripper* m_simulatedGripper;

    std::string m_name;
};

///
/// \brief This is a higher level class that queries the number of haptics devices available on the sytem
/// and on the Network for dVRK devices and creates a single Bullet Gripper and a Device Handle for
/// each device.
///
class Coordination{
public:
    Coordination(cBulletWorld* a_bullet_world, int a_max_load_devs = MAX_DEVICES);
    ~Coordination();
    SimulatedGripper* create_simulated_gripper(uint dev_num, PhysicalDevice* a_physicalDevice);
    void close_devices();

    double increment_K_lh(double a_offset);
    double increment_K_ah(double a_offset);
    double increment_K_lc(double a_offset);
    double increment_K_ac(double a_offset);
    double increment_B_lc(double a_offset);
    double increment_B_ac(double a_offset);
    bool are_all_haptics_loop_exec();
    int num_of_haptics_loop_execd();
    void clear_all_haptics_loop_exec_flags();

    void next_mode();
    void prev_mode();

    std::shared_ptr<cHapticDeviceHandler> m_deviceHandler;
    std::vector<DeviceGripperPair> m_deviceGripperPairs;

    uint m_num_devices;
    uint m_num_grippers;
    cBulletWorld* m_bulletWorld;

    std::vector<DeviceGripperPair> get_device_gripper_pairs(std::vector<std::string> a_device_names);
    std::vector<DeviceGripperPair> get_all_device_gripper_pairs();

    // bool to enable the rotation of simulated gripper be in camera frame. i.e. Orienting the camera
    // re-orients the simulate gripper.
    bool m_use_cam_frame_rot;
    MODES m_simModes;
    std::string m_mode_str;
    std::vector<MODES> m_modes_enum_vec {MODES::CAM_CLUTCH_CONTROL,
                MODES::GRIPPER_JAW_CONTROL,
                MODES::CHANGE_CONT_LIN_GAIN,
                MODES::CHANGE_CONT_ANG_GAIN,
                MODES::CHANGE_CONT_LIN_DAMP,
                MODES::CHANGE_CONT_ANG_DAMP,
                MODES::CHANGE_DEV_LIN_GAIN,
                MODES::CHANGE_DEV_ANG_GAIN};

    std::vector<std::string> m_modes_enum_str {"CAM_CLUTCH_CONTROL  ",
                                               "GRIPPER_JAW_CONTROL ",
                                               "CHANGE_CONT_LIN_GAIN",
                                               "CHANGE_CONT_ANG_GAIN",
                                               "CHANGE_CONT_LIN_DAMP",
                                               "CHANGE_CONT_ANG_DAMP",
                                               "CHANGE_DEV_LIN_GAIN ",
                                               "CHANGE_DEV_ANG_GAIN "};
    int m_mode_idx;
};

///
/// \brief Coordination::Coordination
/// \param a_bullet_world
/// \param a_max_load_devs
///
Coordination::Coordination(cBulletWorld* a_bullet_world, int a_max_load_devs){
    m_bulletWorld = NULL;
    m_deviceHandler = NULL;
    m_bulletWorld = a_bullet_world;
    if (a_max_load_devs > 0){
        m_deviceHandler.reset(new cHapticDeviceHandler());
        int numDevs = m_deviceHandler->getNumDevices();
        m_num_devices = a_max_load_devs < numDevs ? a_max_load_devs : numDevs;
        m_num_grippers = m_num_devices;
        std::cerr << "Num of devices " << m_num_devices << std::endl;
        for (uint devIdx = 0; devIdx < m_num_grippers; devIdx++){

            PhysicalDevice* physicalDevice = new PhysicalDevice();
            m_deviceHandler->getDeviceSpecifications(physicalDevice->m_hInfo, devIdx);
            if(m_deviceHandler->getDevice(physicalDevice->m_hDevice, devIdx)){
                physicalDevice->m_hDevice->open();
                std::string name = "Device" + std::to_string(devIdx+1);
                physicalDevice->create_af_cursor(m_bulletWorld, name);

                SimulatedGripper* simulatedGripper = create_simulated_gripper(devIdx, physicalDevice);
                if (simulatedGripper == NULL){
                    m_num_grippers--;
                }
                else{
                    DeviceGripperPair dgPair;
                    dgPair.m_physicalDevice = physicalDevice;
                    dgPair.m_simulatedGripper = simulatedGripper;
                    dgPair.m_name = physicalDevice->m_hInfo.m_modelName;
                    m_deviceGripperPairs.push_back(dgPair);
                }
            }
        }
    }
    else{
        m_num_devices = 0;
        m_num_grippers = 0;
    }
    m_use_cam_frame_rot = true;
    m_simModes = CAM_CLUTCH_CONTROL;
    m_mode_str = "CAM_CLUTCH_CONTROL";
    m_mode_idx = 0;
}

///
/// \brief Coordination::~Coordination
///
Coordination::~Coordination(){
}

///
/// \brief Coordination::get_device_gripper_pairs
/// \return
///
std::vector<DeviceGripperPair> Coordination::get_device_gripper_pairs(std::vector<std::string> a_device_names){
    std::vector<DeviceGripperPair> dgPairs;
    std::vector<DeviceGripperPair>::iterator dgIt = m_deviceGripperPairs.begin();
    for(int nameIdx = 0 ; nameIdx < a_device_names.size() ; nameIdx++){
        std::string dev_name = a_device_names[nameIdx];
        for(; dgIt != m_deviceGripperPairs.end() ; ++dgIt){
            if( dgIt->m_name.compare(dev_name) == 0 ){
                dgPairs.push_back(*dgIt);
            }
            else{
                cerr << "INFO, DEVICE GRIPPER PAIR: \"" << dev_name << "\" NOT FOUND" << endl;
            }
        }
    }

    return dgPairs;

}

///
/// \brief Coordination::get_all_device_gripper_pairs
/// \return
///
std::vector<DeviceGripperPair> Coordination::get_all_device_gripper_pairs(){
    return m_deviceGripperPairs;
}


///
/// \brief Coordination::next_mode
///
void Coordination::next_mode(){
    m_mode_idx = (m_mode_idx + 1) % m_modes_enum_vec.size();
    m_simModes = m_modes_enum_vec[m_mode_idx];
    m_mode_str = m_modes_enum_str[m_mode_idx];
    g_btn_action_str = "";
    g_cam_btn_pressed = false;
    g_clutch_btn_pressed = false;
    std::cout << m_mode_str << std::endl;
}

///
/// \brief Coordination::prev_mode
///
void Coordination::prev_mode(){
    m_mode_idx = (m_mode_idx - 1) % m_modes_enum_vec.size();
    m_simModes = m_modes_enum_vec[m_mode_idx];
    m_mode_str = m_modes_enum_str[m_mode_idx];
    g_btn_action_str = "";
    g_cam_btn_pressed = false;
    g_clutch_btn_pressed = false;
    std::cout << m_mode_str << std::endl;
}

///
/// \brief Coordination::create_simulated_gripper
/// \param dev_num
/// \param a_physicalDevice
/// \return
///
SimulatedGripper* Coordination::create_simulated_gripper(uint dev_num, PhysicalDevice* a_physicalDevice){
    std::ostringstream dev_str;
    dev_str << (dev_num + 1);
    std::string gripper_name = "Gripper" + dev_str.str();
    SimulatedGripper* simulatedGripper = new SimulatedGripper(m_bulletWorld);
    if(simulatedGripper->loadFromAMBF(gripper_name, a_physicalDevice->m_hInfo.m_modelName)){
        simulatedGripper->set_sim_params(a_physicalDevice->m_hInfo, a_physicalDevice);
        a_physicalDevice->m_workspace_scale_factor = simulatedGripper->get_workspace_scale_factor();
        cVector3d localGripperPos = simulatedGripper->m_rootLink->getLocalPos();
        double l,w,h;
        simulatedGripper->getEnclosureExtents(l,w,h);
        if (localGripperPos.length() == 0.0){
            double x = (int(dev_num / 2.0) * 0.8);
            double y = (dev_num % 2) ? +0.4 : -0.4;
            x /= simulatedGripper->m_workspaceScaleFactor;
            y /= simulatedGripper->m_workspaceScaleFactor;
            simulatedGripper->m_posRefOrigin.set(x, y, 0);
        }
        return simulatedGripper;
    }
    else{
        delete simulatedGripper;
        return NULL;
    }
}

///
/// \brief Coordination::close_devices
///
void Coordination::close_devices(){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        m_deviceGripperPairs[devIdx].m_physicalDevice->m_hDevice->close();
    }
}

///
/// \brief Coordination::num_of_haptics_loop_execd
/// \return
///
int Coordination::num_of_haptics_loop_execd(){
    int num_devs_loop_execd = 0;
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if ( m_deviceGripperPairs[devIdx].m_simulatedGripper->is_loop_exec()) num_devs_loop_execd++;
    }
    return num_devs_loop_execd;
}

///
/// \brief Coordination::are_all_haptics_loop_exec
/// \return
///
bool Coordination::are_all_haptics_loop_exec(){
    bool flag = true;
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        flag &= m_deviceGripperPairs[devIdx].m_simulatedGripper->is_loop_exec();
    }
    return flag;
}

///
/// \brief Coordination::clear_all_haptics_loop_exec_flags
///
void Coordination::clear_all_haptics_loop_exec_flags(){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        m_deviceGripperPairs[devIdx].m_simulatedGripper->clear_loop_exec_flag();
    }
}

///
/// \brief Coordination::increment_K_lh
/// \param a_offset
/// \return
///
double Coordination::increment_K_lh(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lh + a_offset <= 0)
        {
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lh = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lh += a_offset;
        }
    }
    //Set the return value to the gain of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->K_lh;
        g_btn_action_str = "K_lh = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief Coordination::increment_K_ah
/// \param a_offset
/// \return
///
double Coordination::increment_K_ah(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ah + a_offset <=0){
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ah = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ah += a_offset;
        }
    }
    //Set the return value to the gain of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->K_ah;
        g_btn_action_str = "K_ah = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief Coordination::increment_K_lc
/// \param a_offset
/// \return
///
double Coordination::increment_K_lc(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lc + a_offset <=0){
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lc = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_lc += a_offset;
        }
    }
    //Set the return value to the stiffness of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->K_lc;
        g_btn_action_str = "K_lc = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief Coordination::increment_K_ac
/// \param a_offset
/// \return
///
double Coordination::increment_K_ac(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ac + a_offset <=0){
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ac = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->K_ac += a_offset;
        }
    }
    //Set the return value to the stiffness of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->K_ac;
        g_btn_action_str = "K_ac = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief Coordination::increment_B_lc
/// \param a_offset
/// \return
///
double Coordination::increment_B_lc(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->B_lc + a_offset <=0){
            m_deviceGripperPairs[devIdx].m_simulatedGripper->B_lc = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->B_lc += a_offset;
        }
    }
    //Set the return value to the stiffness of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->B_lc;
        g_btn_action_str = "B_lc = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief Coordination::increment_B_ac
/// \param a_offset
/// \return
///
double Coordination::increment_B_ac(double a_offset){
    for (int devIdx = 0 ; devIdx < m_num_grippers ; devIdx++){
        if (m_deviceGripperPairs[devIdx].m_simulatedGripper->B_ac + a_offset <=0){
            m_deviceGripperPairs[devIdx].m_simulatedGripper->B_ac = 0.0;
        }
        else{
            m_deviceGripperPairs[devIdx].m_simulatedGripper->B_ac += a_offset;
        }
    }
    //Set the return value to the stiffness of the last device
    if(m_num_grippers > 0){
        a_offset = m_deviceGripperPairs[m_num_grippers-1].m_simulatedGripper->B_ac;
        g_btn_action_str = "B_ac = " + cStr(a_offset, 4);
    }
    return a_offset;
}

///
/// \brief This is an implementation of Sleep function that tries to adjust sleep between each cycle to maintain
/// the desired loop frequency. This class has been inspired from ROS Rate Sleep written by Eitan Marder-Eppstein
///
class RateSleep{
public:
    RateSleep(int a_freq){
        m_cycle_time = 1.0 / double(a_freq);
        m_rateClock.start();
        m_next_expected_time = m_rateClock.getCurrentTimeSeconds() + m_cycle_time;
    }
    bool sleep(){
        double cur_time = m_rateClock.getCurrentTimeSeconds();
        if (cur_time >= m_next_expected_time){
            m_next_expected_time = cur_time + m_cycle_time;
            return true;
        }
        while(m_rateClock.getCurrentTimeSeconds() <= m_next_expected_time){

        }
        m_next_expected_time = m_rateClock.getCurrentTimeSeconds() + m_cycle_time;
        return true;
    }
private:
    double m_next_expected_time;
    double m_cycle_time;
    cPrecisionClock m_rateClock;
};


std::shared_ptr<Coordination> g_coordApp;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

///
/// \brief main: This Application allows multi-manual tasks using several haptics devices.
/// Each device can perturb or control the dynamic bodies in the simulation
/// environment. The objects in the simulation are exposed via Asynchoronous
/// Framework (AMBF) to allow query and control via external applications.
/// \param argc
/// \param argv
/// \return
///
int main(int argc, char* argv[])
{
    //-----------------------------------------------------------------------
    // INITIALIZATION
    //-----------------------------------------------------------------------
    namespace p_opt = boost::program_options;

    p_opt::options_description cmd_opts("Coordination Application Usage");
    cmd_opts.add_options()
            ("help,h", "Show help")
            ("ndevs,n", p_opt::value<int>(), "Number of Haptic Devices to Load")
            ("timestep,t", p_opt::value<double>(), "Value in secs for fixed Simulation time step(dt)")
            ("enableforces,f", p_opt::value<bool>(), "Enable Force Feedback on Devices");
    p_opt::variables_map var_map;
    p_opt::store(p_opt::command_line_parser(argc, argv).options(cmd_opts).run(), var_map);
    p_opt::notify(var_map);

    int num_devices_to_load = MAX_DEVICES;
    if(var_map.count("help")){ std::cout<< cmd_opts << std::endl; return 0;}
    if(var_map.count("ndevs")){ num_devices_to_load = var_map["ndevs"].as<int>();}
    if (var_map.count("timestep")){ g_dt_fixed = var_map["timestep"].as<double>();}
    if (var_map.count("enableforces")){ g_force_enable = var_map["enableforces"].as<bool>();}

    cout << endl;
    cout << "____________________________________________________________" << endl << endl;
    cout << "ASYNCHRONOUS MULTI-BODY FRAMEWORK SIMULATOR (AMBF Simulator)" << endl;
    cout << endl << endl;
    cout << "\t\t(www.aimlab.wpi.edu)" << endl;
    cout << "\t\t  (Copyright 2019)" << endl;
    cout << "____________________________________________________________" << endl << endl;
    cout << "STARTUP COMMAND LINE OPTIONS: " << endl << endl;
    cout << "-t <float> simulation dt in seconds, (default: Dynamic RT)" << endl;
    cout << "-f <bool> enable haptic feedback, (default: True)" << endl;
    cout << "-n <int> max devices to load, (default: All Available)" << endl;
    cout << "------------------------------------------------------------" << endl << endl << endl;
    cout << endl;

    //-----------------------------------------------------------------------
    // OPEN GL - WINDOW DISPLAY
    //-----------------------------------------------------------------------

    // initialize GLFW library
    if (!glfwInit())
    {
        cout << "failed initialization" << endl;
        cSleepMs(1000);
        return 1;
    }

    // set error callback
    glfwSetErrorCallback(errorCallback);

    // compute desired size of window
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int w = 0.8 * mode->height;
    int h = 0.5 * mode->height;
    int x = 0.5 * (mode->width - w);
    int y = 0.5 * (mode->height - h);

    // set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // set active stereo mode
    if (stereoMode == C_STEREO_ACTIVE)
    {
        glfwWindowHint(GLFW_STEREO, GL_TRUE);
    }
    else
    {
        glfwWindowHint(GLFW_STEREO, GL_FALSE);
    }


    //-----------------------------------------------------------------------
    // 3D - SCENEGRAPH
    //-----------------------------------------------------------------------

    // create a dynamic world.
    g_bulletWorld = new cBulletWorld("World");

    // set the background color of the environment
    g_bulletWorld->m_backgroundColor.setWhite();

    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    cFontPtr font = NEW_CFONTCALIBRI20();

    // create a label to display the haptic and graphic rate of the simulation
    g_labelRates = new cLabel(font);
    g_labelTimes = new cLabel(font);
    g_labelModes = new cLabel(font);
    g_labelBtnAction = new cLabel(font);
    g_labelRates->m_fontColor.setBlack();
    g_labelTimes->m_fontColor.setBlack();
    g_labelModes->m_fontColor.setBlack();
    g_labelBtnAction->m_fontColor.setBlack();

    //////////////////////////////////////////////////////////////////////////
    // BULLET WORLD
    //////////////////////////////////////////////////////////////////////////
    // set some gravity
    g_bulletWorld->setGravity(cVector3d(0.0, 0.0, -9.8));


    //////////////////////////////////////////////////////////////////////////
    // AF MULTIBODY HANDLER
    //////////////////////////////////////////////////////////////////////////
    g_afWorld = new afWorld(g_bulletWorld);
    if (g_afWorld->loadBaseConfig("../../ambf_models/descriptions/launch.yaml")){
        g_afWorld->loadWorld();

        g_afMultiBody = new afMultiBody(g_bulletWorld);
    //    g_afMultiBody->loadMultiBody();
        g_afMultiBody->loadAllMultiBodies();
    }

    // end puzzle meshes
    //////////////////////////////////////////////////////////////////////////
    // INVISIBLE WALLS
    //////////////////////////////////////////////////////////////////////////
    // we create 5 static walls to contain the dynamic objects within a limited workspace
    double box_l, box_w, box_h;
    box_l = g_afWorld->getEnclosureLength();
    box_w = g_afWorld->getEnclosureWidth();
    box_h = g_afWorld->getEnclosureHeight();
    g_bulletBoxWallZ[0] = new cBulletStaticPlane(g_bulletWorld, cVector3d(0.0, 0.0, -1.0), -0.5 * box_h);
    g_bulletBoxWallY[0] = new cBulletStaticPlane(g_bulletWorld, cVector3d(0.0, -1.0, 0.0), -0.5 * box_w);
    g_bulletBoxWallY[1] = new cBulletStaticPlane(g_bulletWorld, cVector3d(0.0, 1.0, 0.0), -0.5 * box_w);
    g_bulletBoxWallX[0] = new cBulletStaticPlane(g_bulletWorld, cVector3d(-1.0, 0.0, 0.0), -0.5 * box_l);
    g_bulletBoxWallX[1] = new cBulletStaticPlane(g_bulletWorld, cVector3d(1.0, 0.0, 0.0), -0.5 * box_l);

    if (g_afWorld->m_lights.size() > 0){
        for (size_t ligth_idx = 0; ligth_idx < g_afWorld->m_lights.size(); ligth_idx++){
            cSpotLight* lightPtr = new cSpotLight(g_bulletWorld);
            afLight light_data = *(g_afWorld->m_lights[ligth_idx]);
            lightPtr->setLocalPos(light_data.m_location);
            lightPtr->setDir(light_data.m_direction);
            lightPtr->setSpotExponent(light_data.m_spot_exponent);
            lightPtr->setCutOffAngleDeg(light_data.m_cuttoff_angle * (180/3.14));
            lightPtr->setShadowMapEnabled(true);
            switch (light_data.m_shadow_quality) {
            case ShadowQuality::no_shadow:
                lightPtr->setShadowMapEnabled(false);
                break;
            case ShadowQuality::very_low:
                lightPtr->m_shadowMap->setQualityVeryLow();
                break;
            case ShadowQuality::low:
                lightPtr->m_shadowMap->setQualityLow();
                break;
            case ShadowQuality::medium:
                lightPtr->m_shadowMap->setQualityMedium();
                break;
            case ShadowQuality::high:
                lightPtr->m_shadowMap->setQualityHigh();
                break;
            case ShadowQuality::very_high:
                lightPtr->m_shadowMap->setQualityVeryHigh();
                break;
            }
            lightPtr->setEnabled(true);
            g_bulletWorld->addChild(lightPtr);
        }
    }
    else{
        std::cerr << "INFO: NO LIGHT SPECIFIED, USING DEFAULT LIGHT" << std::endl;
        cSpotLight* lightPtr = new cSpotLight(g_bulletWorld);
        lightPtr->setLocalPos(cVector3d(0.0, 0.5, 2.5));
        lightPtr->setDir(0, 0, -1);
        lightPtr->setSpotExponent(0.3);
        lightPtr->setCutOffAngleDeg(60);
        lightPtr->setShadowMapEnabled(true);
        lightPtr->m_shadowMap->setQualityVeryHigh();
        lightPtr->setEnabled(true);
        g_bulletWorld->addChild(lightPtr);
    }

    if (g_afWorld->m_cameras.size() > 0){
        int num_monitors;
        GLFWmonitor** monitors = glfwGetMonitors(&num_monitors);
        for (size_t camera_idx = 0; camera_idx < g_afWorld->m_cameras.size(); camera_idx++){
            cCamera* cameraPtr = new cCamera(g_bulletWorld);
            afCamera camera_data = *(g_afWorld->m_cameras[camera_idx]);

            // set camera name
            cameraPtr->m_name = camera_data.m_name;

            cameraPtr->set(camera_data.m_location, camera_data.m_look_at, camera_data.m_up);
            cameraPtr->setClippingPlanes(camera_data.m_clipping_plane_limits[0], camera_data.m_clipping_plane_limits[1]);
            cameraPtr->setFieldViewAngleRad(camera_data.m_field_view_angle);
            if (camera_data.m_enable_ortho_view){
                cameraPtr->setOrthographicView(camera_data.m_ortho_view_width);
            }
//            cameraPtr->setEnabled(true);
            cameraPtr->setStereoMode(stereoMode);
            cameraPtr->setStereoEyeSeparation(0.02);
            cameraPtr->setStereoFocalLength(2.0);
            cameraPtr->setMirrorVertical(mirroredDisplay);

            g_bulletWorld->addChild(cameraPtr);

            cameraPtr->m_frontLayer->addChild(g_labelRates);
            cameraPtr->m_frontLayer->addChild(g_labelTimes);
            cameraPtr->m_frontLayer->addChild(g_labelModes);
            cameraPtr->m_frontLayer->addChild(g_labelBtnAction);

            std::string window_name = "AMBF Simulator Window " + std::to_string(camera_idx + 1);
            // create display context
            int monitor_to_load = 0;
            if (camera_idx < num_monitors){
                monitor_to_load = camera_idx;
            }
            GLFWwindow* windowPtr = glfwCreateWindow(w, h, window_name.c_str() , NULL, NULL);

            // Assign the Window Camera Handles
            WindowCameraHandle winCamHandle;
            winCamHandle.m_window = windowPtr;
            winCamHandle.m_monitor = monitors[monitor_to_load];
            winCamHandle.m_camera = cameraPtr;
            winCamHandle.m_deviceNames = camera_data.m_controlling_devices;
            g_windowCameraHandles.push_back(winCamHandle);
        }
    }
    else{
        std::cerr << "INFO: NO CAMERA SPECIFIED, USING DEFAULT CAMERA" << std::endl;
        cCamera* cameraPtr = new cCamera(g_bulletWorld);
        g_bulletWorld->addChild(cameraPtr);

        // position and orient the camera
        cameraPtr->set(cVector3d(4.0, 0.0, 2.0),    // camera position (eye)
                      cVector3d(0.0, 0.0,-0.5),    // lookat position (target)
                      cVector3d(0.0, 0.0, 1.0));   // direction of the "up" vector

        // set the near and far clipping planes of the camera
        cameraPtr->setClippingPlanes(0.01, 10.0);

        // set stereo mode
        cameraPtr->setStereoMode(stereoMode);

        // set stereo eye separation and focal length (applies only if stereo is enabled)
        cameraPtr->setStereoEyeSeparation(0.02);
        cameraPtr->setStereoFocalLength(2.0);

        // set vertical mirrored display mode
        cameraPtr->setMirrorVertical(mirroredDisplay);

        // create display context
        GLFWwindow* windowPtr = glfwCreateWindow(w, h, "AMBF Simulator", NULL, NULL);
        GLFWmonitor* monitorPtr = glfwGetPrimaryMonitor();

        // Assign the Window Camera Handles
        WindowCameraHandle winCamHandle;
        winCamHandle.m_window = windowPtr;
        winCamHandle.m_monitor = monitorPtr;
        winCamHandle.m_camera = cameraPtr;
        g_windowCameraHandles.push_back(winCamHandle);
    }

    g_winCamIt = g_windowCameraHandles.begin();
    for(; g_winCamIt != g_windowCameraHandles.end() ; ++g_winCamIt){
        GLFWwindow* windowPtr = g_winCamIt->m_window;
        if (!windowPtr)
        {
            cout << "failed to create window" << endl;
            cSleepMs(1000);
            glfwTerminate();
            return 1;
        }

        // get width and height of window
        glfwGetWindowSize(windowPtr, &g_width, &g_height);

        // set position of window
        glfwSetWindowPos(windowPtr, x, y);

        // set key callback
        glfwSetKeyCallback(windowPtr, keyCallback);

        // set resize callback
        glfwSetWindowSizeCallback(windowPtr, windowSizeCallback);

        // set the current context
        glfwMakeContextCurrent(windowPtr);

        // sets the swap interval for the current display context
//        glfwSwapInterval(g_swapInterval);
    }

    glfwSwapInterval(g_swapInterval);

    // initialize GLEW library
#ifdef GLEW_VERSION
    if (glewInit() != GLEW_OK)
    {
        cout << "failed to initialize GLEW library" << endl;
        glfwTerminate();
        return 1;
    }
#endif

    cVector3d worldZ(0.0, 0.0, 1.0);
    cMaterial matPlane;
    matPlane.setWhiteIvory();
    matPlane.setShininess(0.3);
    cVector3d planeNorm;
    cMatrix3d planeRot;

    for (int i = 0 ; i < 2 ; i++){
        planeNorm = cCross(g_bulletBoxWallX[i]->getPlaneNormal(), worldZ);
        planeRot.setAxisAngleRotationDeg(planeNorm, 90);
        g_bulletWorld->addChild(g_bulletBoxWallX[i]);
        cCreatePlane(g_bulletBoxWallX[i], box_h, box_w,
                     g_bulletBoxWallX[i]->getPlaneConstant() * g_bulletBoxWallX[i]->getPlaneNormal(),
                     planeRot);
        g_bulletBoxWallX[i]->setMaterial(matPlane);
        if (i == 0) g_bulletBoxWallX[i]->setTransparencyLevel(0.3, true, true);
        else g_bulletBoxWallX[i]->setTransparencyLevel(0.5, true, true);
    }

    for (int i = 0 ; i < 2 ; i++){
        planeNorm = cCross(g_bulletBoxWallY[i]->getPlaneNormal(), worldZ);
        planeRot.setAxisAngleRotationDeg(planeNorm, 90);
        g_bulletWorld->addChild(g_bulletBoxWallY[i]);
        cCreatePlane(g_bulletBoxWallY[i], box_l, box_h,
                     g_bulletBoxWallY[i]->getPlaneConstant() * g_bulletBoxWallY[i]->getPlaneNormal(),
                     planeRot);
        g_bulletBoxWallY[i]->setMaterial(matPlane);
        g_bulletBoxWallY[i]->setTransparencyLevel(0.5, true, true);
    }


    //////////////////////////////////////////////////////////////////////////
    // GROUND
    //////////////////////////////////////////////////////////////////////////

    // create ground plane
    g_bulletGround = new cBulletStaticPlane(g_bulletWorld, cVector3d(0.0, 0.0, 1.0), -0.5 * box_h);

    // add plane to world as we will want to make it visibe
    g_bulletWorld->addChild(g_bulletGround);

    // create a mesh plane where the static plane is located
    cCreatePlane(g_bulletGround, box_l + 0.4, box_w + 0.8, g_bulletGround->getPlaneConstant() * g_bulletGround->getPlaneNormal());

    // define some material properties and apply to mesh
    cMaterial matGround;
    matGround.setGreenChartreuse();
    matGround.m_emission.setGrayLevel(0.3);
    g_bulletGround->setMaterial(matGround);
    g_bulletGround->m_bulletRigidBody->setFriction(1.0);

    //-----------------------------------------------------------------------
    // START SIMULATION
    //-----------------------------------------------------------------------
    g_coordApp = std::make_shared<Coordination>(g_bulletWorld, num_devices_to_load);

    // create a thread which starts the main haptics rendering loop
    int dev_num[10] = {0,1,2,3,4,5,6,7,8,9};
    int num_grippers = g_coordApp->m_num_grippers;
    for (int gIdx = 0 ; gIdx < num_grippers ; gIdx++){
        g_hapticsThreads.push_back(new cThread());
        g_hapticsThreads[gIdx]->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS, &dev_num[gIdx]);
        m_labelDevRates.push_back(new cLabel(font));
        m_labelDevRates[gIdx]->m_fontColor.setBlack();
        m_labelDevRates[gIdx]->setFontScale(0.8);
        g_winCamIt = g_windowCameraHandles.begin();
        for (;  g_winCamIt !=  g_windowCameraHandles.end() ; ++ g_winCamIt){
            g_winCamIt->m_camera->m_frontLayer->addChild(m_labelDevRates[gIdx]);
            g_winCamIt->m_cam_rot_last.resize(num_grippers);
            g_winCamIt->m_dev_rot_last.resize(num_grippers);
            g_winCamIt->m_dev_rot_cur.resize(num_grippers);
            g_winCamIt->m_cam_pressed.resize(num_grippers);
            if(g_winCamIt->m_deviceNames.size() == 0){
                g_winCamIt->m_dgPair  = g_coordApp->get_all_device_gripper_pairs();
            }
            else{
                g_winCamIt->m_dgPair  = g_coordApp->get_device_gripper_pairs(g_winCamIt->m_deviceNames);
            }
        }

    }

    //create a thread which starts the Bullet Simulation loop
    g_bulletSimThread = new cThread();
    g_bulletSimThread->start(updateBulletSim, CTHREAD_PRIORITY_HAPTICS);

    // setup callback when application exits
    atexit(close);


    //--------------------------------------------------------------------------
    // MAIN GRAPHIC LOOP
    //--------------------------------------------------------------------------

    // call window size callback at initialization
    g_winCamIt = g_windowCameraHandles.begin();
    for (; g_winCamIt != g_windowCameraHandles.end() ; ++ g_winCamIt){
        windowSizeCallback(g_winCamIt->m_window, g_width, g_height);
    }
    bool _window_closed = false;
    // main graphic loop
    while (!_window_closed)
    {
        g_winCamIt = g_windowCameraHandles.begin();
        for (; g_winCamIt != g_windowCameraHandles.end(); ++ g_winCamIt){
            // set current display context
            glfwMakeContextCurrent(g_winCamIt->m_window);

            // get width and height of window
            glfwGetWindowSize(g_winCamIt->m_window, &g_width, &g_height);

            // render graphics
            updateGraphics(*g_winCamIt);

            // swap buffers
            glfwSwapBuffers(g_winCamIt->m_window);

            // Only set the _window_closed if the condition is met
            // otherwise a non-closed window will set the variable back
            // to false
            if (glfwWindowShouldClose(g_winCamIt->m_window)){
                _window_closed = true;
            }
        }

        g_bulletWorld->updateShadowMaps(false, mirroredDisplay);

        // wait until all GL commands are completed
        glFinish();

        // check for any OpenGL errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) printf("Error:  %s\n", gluErrorString(err));

        // process events
        glfwPollEvents();

        // signal frequency counter
        g_freqCounterGraphics.signal(1);
    }

    // close window
    g_winCamIt = g_windowCameraHandles.begin();
    for (; g_winCamIt !=  g_windowCameraHandles.end() ; ++ g_winCamIt){
        glfwDestroyWindow(g_winCamIt->m_window);
    }


    // terminate GLFW library
    glfwTerminate();

    // exit
    return 0;
}

///
/// \brief windowSizeCallback
/// \param a_window
/// \param a_width
/// \param a_height
///
void windowSizeCallback(GLFWwindow* a_window, int a_width, int a_height)
{
    // update window size
    g_width = a_width;
    g_height = a_height;
}

///
/// \brief errorCallback
/// \param a_error
/// \param a_description
///
void errorCallback(int a_error, const char* a_description)
{
    cout << "Error: " << a_description << endl;
}

///
/// \brief keyCallback
/// \param a_window
/// \param a_key
/// \param a_scancode
/// \param a_action
/// \param a_mods
///
void keyCallback(GLFWwindow* a_window, int a_key, int a_scancode, int a_action, int a_mods)
{
    // filter calls that only include a key press
    if ((a_action != GLFW_PRESS) && (a_action != GLFW_REPEAT))
    {
        return;
    }

    // option - exit
    else if ((a_key == GLFW_KEY_ESCAPE) || (a_key == GLFW_KEY_Q))
    {
        glfwSetWindowShouldClose(a_window, GLFW_TRUE);
    }

    // option - toggle fullscreen
    else if (a_key == GLFW_KEY_F)
    {
        // toggle state variable
        fullscreen = !fullscreen;

        std::vector<WindowCameraHandle>::iterator win_cam_it;
        for (win_cam_it = g_windowCameraHandles.begin() ; win_cam_it != g_windowCameraHandles.end() ; ++win_cam_it){

            // get handle to monitor
            GLFWmonitor* monitor = win_cam_it->m_monitor;

            // get information about monitor
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);

            // set fullscreen or window mode
            if (fullscreen)
            {
                glfwSetWindowMonitor(win_cam_it->m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                glfwSwapInterval(win_cam_it->m_swapInterval);
            }
            else
            {
                int w = 0.8 * mode->height;
                int h = 0.5 * mode->height;
                int x = 0.5 * (mode->width - w);
                int y = 0.5 * (mode->height - h);
                glfwSetWindowMonitor(win_cam_it->m_window, NULL, x, y, w, h, mode->refreshRate);
                glfwSwapInterval(win_cam_it->m_swapInterval);
            }
        }
    }

    // option - toggle vertical mirroring
    else if (a_key == GLFW_KEY_M)
    {
         mirroredDisplay = !mirroredDisplay;
        std::vector<WindowCameraHandle>::iterator win_cam_it;
        for (win_cam_it = g_windowCameraHandles.begin() ; win_cam_it != g_windowCameraHandles.end() ; ++win_cam_it){
            win_cam_it->m_camera->setMirrorVertical(mirroredDisplay);
        }
    }

    // option - help menu
    else if (a_key == GLFW_KEY_H)
    {
        cout << "Keyboard Options:" << endl << endl;
        cout << "[h] - Display help menu" << endl;
        cout << "[1] - Enable gravity" << endl;
        cout << "[2] - Disable gravity" << endl << endl;
        cout << "[3] - decrease linear haptic gain" << endl;
        cout << "[4] - increase linear haptic gain" << endl;
        cout << "[5] - decrease angular haptic gain" << endl;
        cout << "[6] - increase angular haptic gain" << endl << endl;
        cout << "[7] - decrease linear stiffness" << endl;
        cout << "[8] - increase linear stiffness" << endl;
        cout << "[9] - decrease angular stiffness" << endl;
        cout << "[0] - increase angular stiffness" << endl << endl;
        cout << "[v] - toggle frame visualization" << endl;
        cout << "[s] - toggle wireframe mode for softbody" << endl;
        cout << "[w] - use world frame for orientation clutch" << endl;
        cout << "[c] - use camera frame for orientation clutch" << endl;
        cout << "[n] - next device mode" << endl << endl;
        cout << "[q] - Exit application\n" << endl;
        cout << endl << endl;
    }

    // option - enable gravity
    else if (a_key == GLFW_KEY_1)
    {
        // enable gravity
        g_bulletWorld->setGravity(cVector3d(0.0, 0.0, -9.8));
        printf("gravity ON:\n");
    }

    // option - disable gravity
    else if (a_key == GLFW_KEY_2)
    {
        // disable gravity
        g_bulletWorld->setGravity(cVector3d(0.0, 0.0, 0.0));
        printf("gravity OFF:\n");
    }

    // option - decrease linear haptic gain
    else if (a_key == GLFW_KEY_3)
    {
        printf("linear haptic gain:  %f\n", g_coordApp->increment_K_lh(-0.05));
    }

    // option - increase linear haptic gain
    else if (a_key == GLFW_KEY_4)
    {
        printf("linear haptic gain:  %f\n", g_coordApp->increment_K_lh(0.05));
    }

    // option - decrease angular haptic gain
    else if (a_key == GLFW_KEY_5)
    {
        printf("angular haptic gain:  %f\n", g_coordApp->increment_K_ah(-0.05));
    }

    // option - increase angular haptic gain
    else if (a_key == GLFW_KEY_6)
    {
        printf("angular haptic gain:  %f\n", g_coordApp->increment_K_ah(0.05));
    }

    // option - decrease linear stiffness
    else if (a_key == GLFW_KEY_7)
    {
        printf("linear stiffness:  %f\n", g_coordApp->increment_K_lc(-50));
    }

    // option - increase linear stiffness
    else if (a_key == GLFW_KEY_8)
    {
        printf("linear stiffness:  %f\n", g_coordApp->increment_K_lc(50));
    }

    // option - decrease angular stiffness
    else if (a_key == GLFW_KEY_9)
    {
        printf("angular stiffness:  %f\n", g_coordApp->increment_K_ac(-1));
    }

    // option - increase angular stiffness
    else if (a_key == GLFW_KEY_0)
    {
        printf("angular stiffness:  %f\n", g_coordApp->increment_K_ac(1));
    }
    else if (a_key == GLFW_KEY_C){
        g_coordApp->m_use_cam_frame_rot = true;
        printf("Gripper Rotation w.r.t Camera Frame:\n");
    }
    else if (a_key == GLFW_KEY_W){
        g_coordApp->m_use_cam_frame_rot = false;
        printf("Gripper Rotation w.r.t World Frame:\n");
    }
    else if (a_key == GLFW_KEY_N){
        g_coordApp->next_mode();
        printf("Changing to next device mode:\n");
    }
    else if (a_key == GLFW_KEY_S){
        auto sbMap = g_afMultiBody->getSoftBodyMap();
        afSoftBodyMap::const_iterator sbIt;
        for (sbIt = sbMap->begin() ; sbIt != sbMap->end(); ++sbIt){
            sbIt->second->toggleSkeletalModelVisibility();
        }
    }
    else if (a_key == GLFW_KEY_V){
        auto rbMap = g_afMultiBody->getRigidBodyMap();
        afRigidBodyMap::const_iterator rbIt;
        for (rbIt = rbMap->begin() ; rbIt != rbMap->end(); ++rbIt){
            rbIt->second->toggleFrameVisibility();
        }
    }

}

///
/// \brief close
///
void close(void)
{
    // stop the simulation
    g_simulationRunning = false;

    // wait for graphics and haptics loops to terminate
    while (!g_simulationFinished) { cSleepMs(100); }
    g_bulletSimThread->stop();
    for(int i = 0 ; i < g_coordApp->m_num_grippers ; i ++){
        g_hapticsThreads[i]->stop();
    }

    // delete resources
    g_coordApp->close_devices();
    for(int i = 0 ; i < g_coordApp->m_num_grippers ; i ++){
        delete g_hapticsThreads[i];
    }
    delete g_bulletWorld;
    delete g_afWorld;
    delete g_afMultiBody;
}



void updateGraphics(WindowCameraHandle a_winCamHandle)
{
    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////
    cCamera* devCam = a_winCamHandle.m_camera;
    int n_grippers = a_winCamHandle.m_dgPair.size();

    // update haptic and graphic rate data
    g_labelTimes->setText("Wall Time: " + cStr(g_clockWorld.getCurrentTimeSeconds(),2) + " s" +
                          + " / "+" Simulation Time: " + cStr(g_bulletWorld->getSimulationTime(),2) + " s");
    g_labelRates->setText(cStr(g_freqCounterGraphics.getFrequency(), 0) + " Hz / " + cStr(g_freqCounterHaptics.getFrequency(), 0) + " Hz");
    g_labelModes->setText("MODE: " + g_coordApp->m_mode_str);
    g_labelBtnAction->setText(" : " + g_btn_action_str);

    for (int gIdx = 0 ; gIdx < n_grippers ; gIdx++){
        PhysicalDevice* pDev = a_winCamHandle.m_dgPair[gIdx].m_physicalDevice;
        m_labelDevRates[gIdx]->setText(pDev->m_hInfo.m_modelName + ": "
                                    + cStr(pDev->m_freq_ctr.getFrequency(), 0) + " Hz");
        m_labelDevRates[gIdx]->setLocalPos(10, (int)(g_height - (gIdx+1)*20));
    }

//    updateMesh();

    // update position of label
    g_labelTimes->setLocalPos((int)(0.5 * (g_width - g_labelTimes->getWidth())), 30);
    g_labelRates->setLocalPos((int)(0.5 * (g_width - g_labelRates->getWidth())), 10);
    g_labelModes->setLocalPos((int)(0.5 * (g_width - g_labelModes->getWidth())), 50);
    g_labelBtnAction->setLocalPos((int)(0.5 * (g_width - g_labelModes->getWidth()) + g_labelModes->getWidth()), 50);

    for (int gIdx = 0; gIdx < n_grippers ; gIdx++){
        PhysicalDevice* pDev = a_winCamHandle.m_dgPair[gIdx].m_physicalDevice;
        SimulatedGripper* sGripper = a_winCamHandle.m_dgPair[gIdx].m_simulatedGripper;
        bool _cam_pressed = a_winCamHandle.m_cam_pressed[gIdx];
        pDev->m_hDevice->getUserSwitch(sGripper->act_2_btn,_cam_pressed);
        a_winCamHandle.m_cam_pressed[gIdx] = _cam_pressed;
        if(_cam_pressed && g_coordApp->m_simModes == MODES::CAM_CLUTCH_CONTROL){
            double scale = 0.3;
            g_dev_vel = pDev->measured_lin_vel();
            cerr << "BEFORE " << a_winCamHandle.m_dev_rot_cur[gIdx].getCol0() << endl;
            pDev->m_hDevice->getRotation(a_winCamHandle.m_dev_rot_cur[gIdx]);
            cerr << "AFTER " << a_winCamHandle.m_dev_rot_cur[gIdx].getCol0() << endl;
            devCam->setLocalPos(devCam->getLocalPos() + cMul(scale, cMul(devCam->getGlobalRot(), g_dev_vel)));
            devCam->setLocalRot(a_winCamHandle.m_cam_rot_last[gIdx] * cTranspose(a_winCamHandle.m_dev_rot_last[gIdx]) * a_winCamHandle.m_dev_rot_cur[gIdx]);
        }
        cerr << "AFTER2 " << a_winCamHandle.m_dev_rot_cur[gIdx].getCol0() << endl;
        if(!_cam_pressed){
            a_winCamHandle.m_cam_rot_last[gIdx] = devCam->getGlobalRot();
            pDev->m_hDevice->getRotation(a_winCamHandle.m_dev_rot_last[gIdx]);
        }
    }

    /////////////////////////////////////////////////////////////////////
    // RENDER SCENE
    /////////////////////////////////////////////////////////////////////

    // render world
    devCam->renderView(g_width, g_height);

//    // update shadow maps (if any)
//    g_bulletWorld->updateShadowMaps(false, mirroredDisplay);


}

///
/// \brief Function to fix time dilation
/// \param adjust_int_steps
/// \return
///
double compute_dt(bool adjust_int_steps = false){
    double dt = g_clockWorld.getCurrentTimeSeconds() - g_bulletWorld->getSimulationTime();
    int min_steps = 2;
    int max_steps = 10;
    if (adjust_int_steps){
        if (dt >= g_bulletWorld->getIntegrationTimeStep() * min_steps){
            int int_steps_max =  dt / g_bulletWorld->getIntegrationTimeStep();
            if (int_steps_max > max_steps){
                int_steps_max = max_steps;
            }
            g_bulletWorld->setIntegrationMaxIterations(int_steps_max + min_steps);        }
    }
    return dt;
}

///
/// \brief updateBulletSim
///
void updateBulletSim(){
    g_simulationRunning = true;
    g_simulationFinished = false;

    // start haptic device
    g_clockWorld.start(true);
    // main Bullet simulation loop
    unsigned int n = g_coordApp->m_num_grippers;
    std::vector<cVector3d> dpos, ddpos, dposPre;
    std::vector<cMatrix3d> drot, ddrot, drotPre;

    dpos.resize(n); ddpos.resize(n); dposPre.resize(n);
    drot.resize(n); ddrot.resize(n); drotPre.resize(n);

    for(unsigned int i = 0 ; i < n; i ++){
        dpos[i].set(0,0,0); ddpos[i].set(0,0,0); dposPre[i].set(0,0,0);
        drot[i].identity(); ddrot[i].identity(); drotPre[i].identity();
    }
    double sleepHz;
    if (g_dt_fixed > 0.0)
        sleepHz = (1.0/g_dt_fixed);
    else
        sleepHz= 1000;

    RateSleep rateSleep(sleepHz);
    while(g_simulationRunning)
    {
        g_freqCounterHaptics.signal(1);
        double dt;
        if (g_dt_fixed > 0.0) dt = g_dt_fixed;
        else dt = compute_dt(true);
        for (unsigned int devIdx = 0 ; devIdx < g_coordApp->m_num_grippers ; devIdx++){
            // update position of simulate gripper
            SimulatedGripper * simGripper = g_coordApp->m_deviceGripperPairs[devIdx].m_simulatedGripper;
            simGripper->update_measured_pose();

            dposPre[devIdx] = dpos[devIdx];
            dpos[devIdx] = simGripper->m_posRef - simGripper->m_pos;
            ddpos[devIdx] = (dpos[devIdx] - dposPre[devIdx]) / dt;

            drotPre[devIdx] = drot[devIdx];
            drot[devIdx] = cTranspose(simGripper->m_rot) * simGripper->m_rotRef;
            ddrot[devIdx] = (cTranspose(drot[devIdx]) * drotPre[devIdx]);

            double angle, dangle;
            cVector3d axis, daxis;
            drot[devIdx].toAxisAngle(axis, angle);
            ddrot[devIdx].toAxisAngle(daxis, dangle);

            cVector3d force, torque;

            force = simGripper->K_lc_ramp * (simGripper->K_lc * dpos[devIdx] + (simGripper->B_lc) * ddpos[devIdx]);
            torque = simGripper->K_ac_ramp * ((simGripper->K_ac * angle) * axis);
            simGripper->m_rot.mul(torque);

            simGripper->apply_force(force);
            simGripper->apply_torque(torque);
            simGripper->setGripperAngle(simGripper->m_gripper_angle, dt);

            if (simGripper->K_lc_ramp < 1.0)
            {
                simGripper->K_lc_ramp = simGripper->K_lc_ramp + 0.5 * dt;
            }
            else
            {
                simGripper->K_lc_ramp = 1.0;
            }

            if (simGripper->K_ac_ramp < 1.0)
            {
                simGripper->K_ac_ramp = simGripper->K_ac_ramp + 0.5 * dt;
            }
            else
            {
                simGripper->K_ac_ramp = 1.0;
            }
        }
        g_bulletWorld->updateDynamics(dt, g_clockWorld.getCurrentTimeSeconds(), g_freqCounterHaptics.getFrequency(), g_coordApp->m_num_grippers);
        g_coordApp->clear_all_haptics_loop_exec_flags();
        rateSleep.sleep();
    }
    g_simulationFinished = true;
}

///
/// \brief updateHaptics
/// \param a_arg
///
void updateHaptics(void* a_arg){
    int devIdx = *(int*) a_arg;
    // simulation in now running
    g_simulationRunning = true;
    g_simulationFinished = false;

    // update position and orientation of simulated gripper
    PhysicalDevice *physicalDevice = g_coordApp->m_deviceGripperPairs[devIdx].m_physicalDevice;
    SimulatedGripper* simulatedGripper = g_coordApp->m_deviceGripperPairs[devIdx].m_simulatedGripper;
    physicalDevice->m_posClutched.set(0.0,0.0,0.0);
    physicalDevice->measured_rot();
    physicalDevice->m_rotClutched.identity();
    simulatedGripper->m_rotRefOrigin = physicalDevice->m_rot;

    cVector3d dpos, ddpos, dposLast;
    cMatrix3d drot, ddrot, drotLast;
    dpos.set(0,0,0); ddpos.set(0,0,0); dposLast.set(0,0,0);
    drot.identity(); ddrot.identity(); drotLast.identity();

    double K_lc_offset = 10;
    double K_ac_offset = 1;
    double B_lc_offset = 1;
    double B_ac_offset = 1;
    double K_lh_offset = 5;
    double K_ah_offset = 1;

    double wait_time = 1.0;
    if (std::strcmp(physicalDevice->m_hInfo.m_modelName.c_str(), "Razer Hydra") == 0 ){
        wait_time = 5.0;
    }

    // main haptic simulation loop
    while(g_simulationRunning)
    {
        physicalDevice->m_freq_ctr.signal(1);
        // Adjust time dilation by computing dt from clockWorld time and the simulationTime
        double dt;
        if (g_dt_fixed > 0.0) dt = g_dt_fixed;
        else dt = compute_dt();

        physicalDevice->m_pos = physicalDevice->measured_pos();
        physicalDevice->m_rot = physicalDevice->measured_rot();

        if(simulatedGripper->m_gripper_pinch_btn >= 0){
            if(physicalDevice->is_button_pressed(simulatedGripper->m_gripper_pinch_btn)){
                physicalDevice->enable_force_feedback(true);
            }
        }
        if (physicalDevice->m_hInfo.m_sensedGripper){
            simulatedGripper->m_gripper_angle = physicalDevice->measured_gripper_angle();
        }
        else{
            simulatedGripper->m_gripper_angle = 0.5;
        }

        if(physicalDevice->is_button_press_rising_edge(simulatedGripper->mode_next_btn)) g_coordApp->next_mode();
        if(physicalDevice->is_button_press_rising_edge(simulatedGripper->mode_prev_btn)) g_coordApp->prev_mode();

        bool btn_1_rising_edge = physicalDevice->is_button_press_rising_edge(simulatedGripper->act_1_btn);
        bool btn_1_falling_edge = physicalDevice->is_button_press_falling_edge(simulatedGripper->act_1_btn);
        bool btn_2_rising_edge = physicalDevice->is_button_press_rising_edge(simulatedGripper->act_2_btn);
        bool btn_2_falling_edge = physicalDevice->is_button_press_falling_edge(simulatedGripper->act_2_btn);

        double gripper_offset = 0;
        switch (g_coordApp->m_simModes){
        case MODES::CAM_CLUTCH_CONTROL:
            g_clutch_btn_pressed  = physicalDevice->is_button_pressed(simulatedGripper->act_1_btn);
            g_cam_btn_pressed     = physicalDevice->is_button_pressed(simulatedGripper->act_2_btn);
            if(g_clutch_btn_pressed) g_btn_action_str = "Clutch Pressed";
            if(g_cam_btn_pressed)   {g_btn_action_str = "Cam Pressed";}
            if(btn_1_falling_edge || btn_2_falling_edge) g_btn_action_str = "";
            break;
        case MODES::GRIPPER_JAW_CONTROL:
            if (btn_1_rising_edge) gripper_offset = 0.1;
            if (btn_2_rising_edge) gripper_offset = -0.1;
            simulatedGripper->offset_gripper_angle(gripper_offset);
            break;
        case MODES::CHANGE_CONT_LIN_GAIN:
            if(btn_1_rising_edge) g_coordApp->increment_K_lc(K_lc_offset);
            if(btn_2_rising_edge) g_coordApp->increment_K_lc(-K_lc_offset);
            break;
        case MODES::CHANGE_CONT_ANG_GAIN:
            if(btn_1_rising_edge) g_coordApp->increment_K_ac(K_ac_offset);
            if(btn_2_rising_edge) g_coordApp->increment_K_ac(-K_ac_offset);
            break;
        case MODES::CHANGE_CONT_LIN_DAMP:
            if(btn_1_rising_edge) g_coordApp->increment_B_lc(B_lc_offset);
            if(btn_2_rising_edge) g_coordApp->increment_B_lc(-B_lc_offset);
            break;
        case MODES::CHANGE_CONT_ANG_DAMP:
            if(btn_1_rising_edge) g_coordApp->increment_B_ac(B_ac_offset);
            if(btn_2_rising_edge) g_coordApp->increment_B_ac(-B_ac_offset);
            break;
        case MODES::CHANGE_DEV_LIN_GAIN:
            if(btn_1_rising_edge) g_coordApp->increment_K_lh(K_lh_offset);
            if(btn_2_rising_edge) g_coordApp->increment_K_lh(-K_lh_offset);
            break;
        case MODES::CHANGE_DEV_ANG_GAIN:
            if(btn_1_rising_edge) g_coordApp->increment_K_ah(K_ah_offset);
            if(btn_2_rising_edge) g_coordApp->increment_K_ah(-K_ah_offset);
            break;
        }


        if (g_clockWorld.getCurrentTimeSeconds() < wait_time){
            physicalDevice->m_posClutched = physicalDevice->m_pos;
        }

        if(g_cam_btn_pressed){
            if(simulatedGripper->btn_cam_rising_edge){
                simulatedGripper->btn_cam_rising_edge = false;
                simulatedGripper->m_posRefOrigin = simulatedGripper->m_posRef / simulatedGripper->m_workspaceScaleFactor;
                simulatedGripper->m_rotRefOrigin = simulatedGripper->m_rotRef;
            }
            physicalDevice->m_posClutched = physicalDevice->m_pos;
            physicalDevice->m_rotClutched = physicalDevice->m_rot;
        }
        else{
            simulatedGripper->btn_cam_rising_edge = true;
        }
        if(g_clutch_btn_pressed){
            if(simulatedGripper->btn_clutch_rising_edge){
                simulatedGripper->btn_clutch_rising_edge = false;
                simulatedGripper->m_posRefOrigin = simulatedGripper->m_posRef / simulatedGripper->m_workspaceScaleFactor;
                simulatedGripper->m_rotRefOrigin = simulatedGripper->m_rotRef;
            }
            physicalDevice->m_posClutched = physicalDevice->m_pos;
            physicalDevice->m_rotClutched = physicalDevice->m_rot;
        }
        else{
            simulatedGripper->btn_clutch_rising_edge = true;
        }

        simulatedGripper->m_posRef = simulatedGripper->m_posRefOrigin +
                (g_windowCameraHandles[0].m_camera->getLocalRot() * (physicalDevice->m_pos - physicalDevice->m_posClutched));
        if (!g_coordApp->m_use_cam_frame_rot){
            simulatedGripper->m_rotRef = simulatedGripper->m_rotRefOrigin * g_windowCameraHandles[0].m_camera->getLocalRot() *
                    cTranspose(physicalDevice->m_rotClutched) * physicalDevice->m_rot *
                    cTranspose(g_windowCameraHandles[0].m_camera->getLocalRot());
        }
        else{
            simulatedGripper->m_rotRef = physicalDevice->m_rot;
        }
        simulatedGripper->m_posRef.mul(simulatedGripper->m_workspaceScaleFactor);

        // update position of simulated gripper
        simulatedGripper->update_measured_pose();

        dposLast = dpos;
        dpos = simulatedGripper->m_posRef - simulatedGripper->m_pos;
        ddpos = (dpos - dposLast) / dt;

        drotLast = drot;
        drot = cTranspose(simulatedGripper->m_rot) * simulatedGripper->m_rotRef;
        ddrot = (cTranspose(drot) * drotLast);

        double angle, dangle;
        cVector3d axis, daxis;
        drot.toAxisAngle(axis, angle);
        ddrot.toAxisAngle(daxis, dangle);

        cVector3d force, torque;

        force  = - g_force_enable * simulatedGripper->K_lh_ramp * (simulatedGripper->K_lc * dpos + (simulatedGripper->B_lc) * ddpos);
        torque = - g_force_enable * simulatedGripper->K_ah_ramp * ((simulatedGripper->K_ac * angle) * axis);

        physicalDevice->apply_wrench(force, torque);

        if (simulatedGripper->K_lh_ramp < simulatedGripper->K_lh)
        {
            simulatedGripper->K_lh_ramp = simulatedGripper->K_lh_ramp + 0.1 * dt * simulatedGripper->K_lh;
        }
        else
        {
            simulatedGripper->K_lh_ramp = simulatedGripper->K_lh;
        }

        if (simulatedGripper->K_ah_ramp < simulatedGripper->K_ah)
        {
            simulatedGripper->K_ah_ramp = simulatedGripper->K_ah_ramp + 0.1 * dt * simulatedGripper->K_ah;
        }
        else
        {
            simulatedGripper->K_ah_ramp = simulatedGripper->K_ah;
        }

        simulatedGripper->set_loop_exec_flag();
    }
    // exit haptics thread
}
