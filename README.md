# Voxel Game

- FPS camera
- context refactor? it might be ok atm
- texturing modes: everything is minimum or ones where its just one
   - and other shit like crops

- different blocks modes like walk thru or not, transparency
- chunk paging, loading visible etc.

## Optimization
- Greedy meshing
   - to do texturing: need to do modulo of tex coords in frag shader
   - AO at the same time its a similar calculation

- How much of this data do i actually need to be sending to the GPU, for instance can normals be calced, texture coordinates, can it all just be done in a mesh shader from the raw blocks? nah gotta mesh
- all 1 type chunks

## Micro world gen
- how about cliffs and caves. maybe use a small amount of 3d?
- domain warping?
- ridge noise
- how to do trees? minecraft style or not. ultimately like to not and have some cool L system shit. L system coconut palm tree. oo climbing up it

### Features
- volcano
- terraces
- flat with mounds

## Macro world gen
- maybe diamond square? or some continenty thing or grammar thing... idk
   world map visualizer, make a mesh and have it spinning and stuff. like kart racer character select
- gradient info for erosion, fluid calcs

## Assets
- snow and dirt to spruce up world gen
- wood

## debug info
- looking at block




-----------------
Thu 2 Jul Signoff

 - fixed reticle
   - in the process got testing infrastructure, debuggin infrastructure, 2d image drawing

 - fixed empty chunks not being upgraded to real chunks
   - should we be able to downgrade? I doubt it, who cares
   - though not sure if the optimization does anything as ram usage doesnt go up when we build into a new once empty chunk

 - fixed ram usage
   - now world can be way bigger. greedy meshing will still be a huge improvement

 - block highlighting
   - rn its wireframe, could be a bit prettier, can u make line thicker
   - yes but it still looks shit lol. maybe alpha and solid colour

 - refactoring
   - graphics - input split
   - chunk - world split

Up next:
 * how to turn off vsync?
 * world treadmill: replace chunks as u move through the world
 * 3d perlin worldgen
 * opts backface culling
 * greedy meshing + ao
 * physics, fps cam (lets do arena shooter speed)
 * console



 Fri 3 Jul
 ---------

 I sense the need to refactor world

 why does my rawdogged code crash inside open simplex noise?
 paranoia says that global shittery leads to osn context being uninitialized memory

 how have i broken world gen? idk
 maybe make world very small and print all the slots

 y are the vbos and vaos all 0 lol

 oh now I see why I was doing it that way

 coming up soon noise refactor and stuff
 sick of struct osn making errors lol

 even better refactor, get rid of useless function lol

OK fixed that issue now we are back to good old fashioned segfaults
still in open simplex noise for some reason
is it like the same ptr?

I should print out the ptr

maybe I could refactor noise
is there a good reason to refactor osn context or nah

invalid free?
how is freeing handled when we re generate?
i guess we don

maybe i dont really need to do dynamic memory idk

it looks like it is trying to free an empty chunk or something

blocks->blocks is retarded but what can you do


blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: 0x56226c5705e0
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: 0x56226c586fc0
blocks ptr: 0x56226c5a6060
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
blocks ptr: (nil)
osn ptr: 0x56226c4c3dd0
-y chunk boundary
+y chunk boundary
freeing (nil)

i think its like not even in the array

oh it kinda is but yeah its not really working properly. need to sort that out. It should probably make an odd number of 

we dont need to malloc/free a lot of the time too... memset 0 would be fine

--

probably needs a big refactor of the chunk manager code. So start the player in the middle of the world, generate a chunk around them.
 this code is really a pest lol

 so treadmilling with offsets seems ok its pretty simple
 theres a mapping of the furthest chunks in the rear to the 


 for some reason we are just generating in the positive octrant
 o i see im feeding the wrong values into chunk coordinates

 -128
 -64
 -128

 to

 16
 111
 143
 hmm

 check out the rounding and stuff

 -64 to +64
 -64 to 0

 ok if Y is shorter its actually the Z values getting robbed
 ok now thats kind of correct as i swapped the indexes around

 i think it should want to unload all the X 0 chunk slots and replace them should it not


 ----

 pretty confusing so valgrind says invlaid read of size 8 in free chunk slot but i thought that was ok

 and also its at opensimplex

 but idk if i can trust it

 it does point to open_simplex2 crashing it
 oh my god

 it misbehaving with re generating chunks as well
 ok thats just because I was using wrong function

 then whats the difference between the treadmill and the generate initial
 and also am I freeing to re gen? or does it use lotsa ram?


 maybe chunk manager and world should be split
 making just one 3d array abstraction is probably the go
 and that could have ABC so i dont have to put it everywhere
 maybe I am going out of bounds and trashing my rng context. lol

 value of vector abstraction? its not that ergonomic but its ok. i think spread macro might be an ok idea

 lets call it chunk slot pos and chunk world pos

 we could also do chunk_slot_of(chunk world pos)


 Tue 7 Jul
 ---------
 Ok so theres 2 things. sometimes it starts doing chunks a billion jillion. after a re gen or something

 and also the osn ptr keeps changing. maybe i should be using it with an extra level of indirection.

doing noise refactoring now. Possibly I want to split the nosie params from the context? or maybe i dont. theres a few layers to pass it through down to the chunks. but oh well