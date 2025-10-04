#include "DataDriven.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include "Input.hpp"
#include "../datadriven/nlohmann/json.hpp"


namespace DataDriven
{
	namespace Config
	{
		void loadInputConfig(const std::string& _filename)
		{
			std::ifstream file(filename);
			if(!file.is_open())
			{
				throw std::runtime_error("Could not open config file: " + _filename);
				return;
			}
		
			try
			{
				nlohmann::json jsonFile;
				file >> jsonFile;
				
				if(jsonFile.contains("buttons"))
				{
					for(auto& [codeStr, name] : jsonFile["buttons"].items())
					{
						int code = std::stoi(codeStr));
						Input::addButtonBinding(code, name.get<std::string>());
					}
				}
				
				if(jsonFile.contains("axes"))
				{
					for(auto& [codeStr, name] : jsonFile["axes"].items())
					{
						int code = std::stoi(codeStr));
						Input::addAxisBinding(code, name.get<std::string>());
					}
				}
				
				std::cout << "Loading input config from: " << _filename << std::endl;
			
			} 
			catch (std::exception expt)
			{
				throw std::runtime_error("Failed to parse config file: " + _filename + " -> " + expt.what();
			}
		
		}
	
	}
}
