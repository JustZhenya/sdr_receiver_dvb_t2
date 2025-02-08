/*
 *  Copyright 2025 vladisslav2011@gmail.com.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef ALIGNED_PTR_H
#define ALIGNED_PTR_H

template <typename T> T* get_aligned(T * in, int alignment)
{
    if(ptrdiff_t(in)%alignment != 0)
        return  &in[(alignment-ptrdiff_t(in)%alignment)/sizeof(T)];
    else
        return in;;
};

#endif /* ALIGNED_PTR_H */

