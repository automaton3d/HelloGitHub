// config.cpp

#include "config.h"
#include "model/simulation.h"

#include <fstream>
#include <sstream>
#include <iostream>

Config gConfig;

using namespace automaton;

static bool parseBool(const std::string& v)
{
    return (v == "1" || v == "true" || v == "TRUE");
}

static void trim(std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");

    if (start == std::string::npos)
    {
        s.clear();
        return;
    }

    s = s.substr(start, end - start + 1);
}

static void stripComment(std::string& s)
{
    size_t pos = s.find('#');
    if (pos != std::string::npos)
    {
        s = s.substr(0, pos);
        trim(s);
    }
}

bool loadConfig(const std::string& path)
{
    std::ifstream file(path);

    // Fallback: try parent directory (useful when running from build/)
    std::string actualPath = path;
    if (!file)
    {
        std::string fallback = "../" + path;
        file.open(fallback);
        if (file)
            actualPath = fallback;
    }

    if (!file)
    {
        std::cout << "[Config] File not found: " << path
                  << " (also tried ../" << path << ")" << std::endl;
        return false;
    }

    std::cout << "[Config] Loading: " << actualPath << std::endl;

    // Reset to defaults before loading
    gConfig = Config{};

    std::string line;

    while (std::getline(file, line))
    {
        trim(line);

        if (line.empty())
            continue;

        if (line[0] == '#' || line[0] == '/')
            continue;

        // Parse key=value
        size_t pos = line.find('=');

        if (pos == std::string::npos)
            continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        trim(key);
        stripComment(value);
        trim(value);

        if (key.empty() || value.empty())
            continue;

        // =========================
        // data3D
        // =========================
        if (key == "data3D.wavefront")
            gConfig.data3D[0] = parseBool(value);

        else if (key == "data3D.momentum")
            gConfig.data3D[1] = parseBool(value);

        else if (key == "data3D.spin")
            gConfig.data3D[2] = parseBool(value);

        else if (key == "data3D.sine_mask")
            gConfig.data3D[3] = parseBool(value);

        else if (key == "data3D.hunting")
            gConfig.data3D[4] = parseBool(value);

        else if (key == "data3D.centers")
            gConfig.data3D[5] = parseBool(value);

        else if (key == "data3D.lattice")
            gConfig.data3D[6] = parseBool(value);

        else if (key == "data3D.axes")
            gConfig.data3D[7] = parseBool(value);

        else if (key == "data3D.plane")
            gConfig.data3D[8] = parseBool(value);

        // =========================
        // delays
        // =========================
        else if (key == "delay.convol")
        {
            gConfig.delays.convol = parseBool(value);
            convol_delay = gConfig.delays.convol;
        }

        else if (key == "delay.diffuse")
        {
            gConfig.delays.diffuse = parseBool(value);
            diffuse_delay = gConfig.delays.diffuse;
        }

        else if (key == "delay.reloc")
        {
            gConfig.delays.reloc = parseBool(value);
            reloc_delay = gConfig.delays.reloc;
        }

        // =========================
        // view
        // =========================
        else if (key == "view.zoom")
        {
            gConfig.view.zoom = std::stof(value);
        }

        else if (key == "view.vis_dx")
        {
            gConfig.view.vis_dx = std::stoi(value);
        }

        else if (key == "view.vis_dy")
        {
            gConfig.view.vis_dy = std::stoi(value);
        }

        else if (key == "view.vis_dz")
        {
            gConfig.view.vis_dz = std::stoi(value);
        }

        // =========================
        // projection
        // =========================
        else if (key == "projection.fov")
        {
            gConfig.projection.fov = std::stof(value);
        }

        else if (key == "projection.near")
        {
            gConfig.projection.near_plane = std::stof(value);
        }

        else if (key == "projection.far")
        {
            gConfig.projection.far_plane = std::stof(value);
        }

        else if (key == "projection.perspective")
        {
            gConfig.projection.perspective = parseBool(value);
        }

        // =========================
        // simulation
        // =========================
        else if (key == "simulation.scenario" || key == "scenario")
        {
            gConfig.simulation.scenario = std::stoi(value);
        }

        // =========================
        // tomography
        // =========================
        else if (key == "tomography.enabled")
        {
            gConfig.tomography.enabled = parseBool(value);
        }

        else if (key == "tomography.axis")
        {
            gConfig.tomography.axis = std::stoi(value);
        }

        else if (key == "tomography.slice")
        {
            gConfig.tomography.slice = std::stof(value);
        }

        else if (key == "tomography.invert")
        {
            gConfig.tomography.invert = parseBool(value);
        }

        else if (key == "tomography.animate")
        {
            gConfig.tomography.animate = parseBool(value);
        }

        else if (key == "tomography.thickness")
        {
            gConfig.tomography.thickness = std::stof(value);
        }
    }

    std::cout << "[Config] scenario = "
              << gConfig.simulation.scenario
              << std::endl;

    return true;
}