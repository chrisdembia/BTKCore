function v = btkGetScalarsValues(h) %#ok
%BTKGETSCALARSVALUES Extract scalars' components and store them in a matrix
% 
%  V = BTKGETSCALARSVALUES(H) returns a matrix of real.  The number of rows
%  of this matrix corresponds to the number of frames in the acquisition.
%  The number of columns of this matrix corresponds to the number of 
%  extracted scalars multiplied by 3 (their components X,Y,Z).
%  The biomechanical acquisition handle H is represented as a double
%  and can be only used with the btk* functions.

%  Author: A. Barré
%  Copyright 2009-2014 Biomechanical ToolKit (BTK).

% The following comment, MATLAB compiler pragma, is necessary to avoid 
% compiling this M-file instead of linking against the MEX-file.  Don't remove.
%# mex

error(generatemsgid('NotSupported'),'MEX file for BTKGETSCALARSVALUES not found');

% [EOF] btkGetScalarsValues.m
