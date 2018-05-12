#!/usr/bin/env python3.5

#____________________________________________________

__author__  = 'Y.Nakada <nakada@km.phys.sci.osaka-u.ac.jp>'
__version__ = '2.1'
__date__    = '12 May 2018'

#____________________________________________________

import os

import yaml

import utility

#____________________________________________________

class RunlistManager :

    #____________________________________________________
    def __init__( self, path ) :

        self.__baseName = os.path.splitext( os.path.basename( path ) )[0]
        self.__workdir  = self.getWorkDir( path )
        self.__runlist  = self.makeRunlist( path )


    #____________________________________________________
    def getTag( self ) :

        return self.__baseName


    #____________________________________________________
    def getRunlist( self, index = None ) :

        if index is None :
            return self.__runlist
        elif len( self.__runlist ) > index :
            return self.__runlist[index]
        else :
            return None


    #____________________________________________________
    def getWorkDir( self, path ) :

        data = dict()
        with open( path, 'r' ) as f :
            data = yaml.load( f.read() )

        tmp = os.path.expanduser( data['WORKDIR'] )
        workdir = tmp if os.path.exists( tmp ) and os.path.isdir( tmp ) \
                  else utility.ExitFailure( 'Cannot find directory: ' + tmp )

        return workdir


    #____________________________________________________
    def makeRunlist( self, path ) :

        cdir = os.getcwd()
        raw_runlist = self.decodeRunlist( path )
        os.chdir( self.__workdir ) 

        runlist = list()
        for item in raw_runlist :

            pbin = item[1]['bin'] if os.path.exists( item[1]['bin'] ) \
                    and os.path.isfile( item[1]['bin'] ) \
                    else utility.ExitFailure( 'Cannot find file: ' + item[1]['bin'] )

            pconf = item[1]['conf'] if os.path.exists( item[1]['conf'] ) \
                     and os.path.isfile( item[1]['conf'] ) \
                     else utility.ExitFailure( 'Cannot find file: ' + item[1]['conf'])

            if os.path.exists( item[1]['data'] ) and os.path.isfile( item[1]['data'] ) :
                tmp = os.path.splitext( os.path.basename( item[1]['data'] ) )[0]
                runno = int( tmp[3:8] ) if tmp[3:8].isdigit() else None
            else :
                runno = item[0] if isinstance( item[0], int ) else None

            pdata = self.makeDataPath( item[1]['data'], runno )
            nevents = self.getNEvents( os.path.dirname( os.path.abspath( pdata ) ), runno )

            base = item[0] + os.path.basename( pbin ) if runno is None \
                    else 'run' + '%05d' % runno + os.path.basename( pbin )
            proot = self.makeRootPath( item[1]['root'], base )

            unit = item[1]['unit'] if isinstance( item[1]['unit'], int ) else 0

            runlist.append( [ item[0], pbin, pconf, pdata, proot, unit, nevents ] ) 

        os.chdir( self.__workdir ) 

        return runlist


    #____________________________________________________
    def decodeRunlist( self, path ) :

        if self.__workdir is None :
            self.__workdir = self.getWorkdir( path )

        data = dict()
        with open( path, 'r' ) as f :
            data = yaml.load( f.read() )

        def_set = data['DEFAULT']

        runlist = list()
        for key, parsets in data['RUN'].items() :

            if parsets is None :
                runlist.append( [ key, def_set ] )
            else :
                for par in parsets :
                    tmp = dict()
                    for item in par :
                        tmp =  { def_key: par[def_key] \
                                if def_key in par else def_set[def_key] \
                                for def_key, def_val, in def_set.items() }
                    runlist.append( [ key, tmp ] )

        return runlist


    #____________________________________________________
    def makeDataPath( self, path, runno = None ) :
    
        data_path = None
    
        if not os.path.exists( path ) :
            utility.ExitFailure( 'Cannot find file: ' + path )
        else :
            if os.path.isfile( path ) :
                data_path = os.path.realpath( path )
            elif os.path.isdir( path ) \
                 and not runno is None \
                 and isinstance( runno, int ) :
                tmp = path + '/run' + '%05d' % runno + '.dat.gz'
                data_path = os.path.realpath( tmp ) \
                            if os.path.exists( tmp ) and os.path.isfile( tmp ) \
                    else utility.ExitFailure( 'Cannot find file: ' + tmp )
            else :
                utility.ExitFailure( 'Cannot decide deta file path' )
    
        return data_path
    

    #____________________________________________________
    def makeRootPath( self, path, base = None ) :
    
        root_path = None
    
        if not os.path.exists( path ) :
            dir_path = os.path.dirname( path )
            if os.path.exists( dir_path )\
                    and os.path.isdir( dir_path ):
                root_path = os.path.realpath( path )
            else :
                utility.ExitFailure( 'Cannot find directory: ' + dir_path )
        elif os.path.isfile( path ) :
            root_path = os.path.realpath( path )
        elif os.path.isdir( path ) and not base is None :
            root_path = os.path.realpath( path + '/' + base + '.root' )
        else :
            utility.ExitFailure( 'Cannot decide root file path' )
    
        return root_path


    #__________________________________________________
    def getNEvents( self, path, runno = None ) :

        nevents = None

        if os.path.exists( path ) and os.path.isdir( path ) : 
            reclog_path = path + '/recorder.log'
            if os.path.exists( reclog_path ) and os.path.isfile( reclog_path ) :
                cand = list()
                freclog = open( reclog_path, 'r' )
                for line in freclog :
                    if 5 == line.find( str( runno ) ) :
                        words = line.split()
                        cand.append( words[15] )
                freclog.close()
                
                nevents = int( cand[0] ) if len( cand ) == 1 else None

        return nevents
