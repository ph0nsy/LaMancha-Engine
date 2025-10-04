/*
 * main.cxx
 * 
 * Copyright 2025 ph0nsy <ph0nsy@ph0nsy-HP-Compaq-8510w>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <iostream>
#include <SFML/Graphics.hpp>

int main(int argc, char **argv)
{
	sf::RenderWindow window(sf::VideoMode(400,300), "SFML Test");
	sf::CircleShape shape(100.f);
	shape.setFillColor(sf::Color::Green);
	
	
	while(window.isOpen())
	{
		sf::Event event;
		while(window.pollEvent(event))
		{
			if(event.type == sf::Event::Closed) window.close();
		}
		window.clear();
		window.draw(shape);
		window.display();
	}
	return 0;
}

