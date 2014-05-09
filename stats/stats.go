package main

import (
	"archive/tar"
	"bufio"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"sort"
	"strings"
	"time"
)

type ByteSize float64

const (
	_           = iota
	KB ByteSize = 1 << (10 * iota)
	MB
	GB
)

func (b ByteSize) String() string {
	switch {
	case b >= GB:
		return fmt.Sprintf("%.2fGB", b/GB)
	case b >= MB:
		return fmt.Sprintf("%.2fMB", b/MB)
	case b >= KB:
		return fmt.Sprintf("%.2fKB", b/KB)
	}
	return fmt.Sprintf("%.2fB", b)
}

type Consensus struct {
	Time time.Time
	Ids  []string
	Size int64
}

type Consensuses []Consensus

type ConsensusStats struct {
	Data              Consensuses
	MeanEntrySize     ByteSize
	MeanConsensusSize ByteSize
	MeanDiffSize      ByteSize
	TotalSize         int64
	HoursMissingIds   [][]string
}

func (c Consensuses) Len() int {
	return len(c)
}

func (c Consensuses) Less(i, j int) bool {
	return c[i].Time.Before(c[j].Time)
}

func (c Consensuses) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}

const (
	dateInForm  = "2006-01-02-15-04-05"
	dateOutForm = "2006-01-02-15-04-05"
)

func analyze(cs Consensuses, tr *tar.Reader) Consensuses {
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		}
		if err != nil {
			log.Fatalln(err)
		}
		if hdr.Typeflag != tar.TypeReg {
			continue
		}

		//log.Println(hdr.Name)
		strTime := path.Base(hdr.Name)[:len("YYYY-MM-DD-HH-MM-SS")]
		parsedTime, err := time.Parse(dateInForm, strTime)
		c := Consensus{Time: parsedTime, Size: hdr.Size}
		scanner := bufio.NewScanner(tr)
		for scanner.Scan() {
			text := scanner.Text()
			switch text[0] {
			case 'r':
				fields := strings.Split(text, " ")
				if len(fields) < 3 {
					continue
				}
				c.Ids = append(c.Ids, fields[2])
			default:
				continue
			}
		}
		if err := scanner.Err(); err != nil {
			log.Fatal(err)
		}
		cs = append(cs, c)
	}
	return cs
}

func results(cst ConsensusStats) {
	cs := cst.Data
	if len(cs) == 0 {
		return
	}
	sort.Sort(cs)
	previousTime := cs[0].Time
	lastSeen := make(map[string]time.Time)
	cst.TotalSize = 0
	var meanEntrySize, meanDiffCount int64 = 0, 0
	for _, c := range cs {
		//log.Printf("Doing consensus from %s\n", c.Time.Format(dateOutForm))
		for _, id := range c.Ids {
			t, e := lastSeen[id]
			if e && t != previousTime {
				h := int(c.Time.Sub(t).Hours())
				c := cap(cst.HoursMissingIds)
				if h >= c {
					t := make([][]string, c*2)
					copy(t, cst.HoursMissingIds)
					cst.HoursMissingIds = t
				}
				//log.Printf("%s had been gone for %d hours\n", id, h)
				cst.HoursMissingIds[h] = append(cst.HoursMissingIds[h], id)
			}
			if !e || (e && t != previousTime) {
				meanDiffCount++
			}
			lastSeen[id] = c.Time
		}
		previousTime = c.Time
		cst.TotalSize += c.Size
		meanEntrySize += c.Size / int64(len(c.Ids))
	}
	cst.MeanEntrySize = ByteSize(meanEntrySize) / ByteSize(len(cs))
	cst.MeanDiffSize = (ByteSize(meanDiffCount) / ByteSize(len(cs))) * ByteSize(cst.MeanEntrySize)
	cst.MeanConsensusSize = ByteSize(cst.TotalSize) / ByteSize(len(cs))
	fmt.Printf("Mean consensus entry size in bytes is %s\n", cst.MeanEntrySize.String())
	fmt.Printf("Mean consensus size in Kbytes is %s\n", cst.MeanConsensusSize.String())
	fmt.Printf("Mean diff size in Kbytes is %s\n", cst.MeanDiffSize.String())
	var countAcc int = 0
	consensusSizeInMonth := cst.MeanConsensusSize / ByteSize(cst.TotalSize)
	for h, ids := range cst.HoursMissingIds {
		c := len(ids)
		if c == 0 {
			continue
		}
		// Twice, once for disappearing and once for reappearing
		count := c * 2
		countAcc += count
		countCons := int(ByteSize(countAcc) * consensusSizeInMonth)
		size := ByteSize(count) * cst.MeanEntrySize
		sizeAcc := ByteSize(countAcc) * cst.MeanEntrySize
		sizeCons := sizeAcc * consensusSizeInMonth
		percentInc := (size / ByteSize(cst.TotalSize)) * 100.0
		if percentInc < 0.002 {
			continue
		}
		percentTotal := (sizeAcc / ByteSize(cst.TotalSize)) * 100.0
		percentDiff := (sizeCons / ByteSize(cst.MeanDiffSize)) * 100.0

		fmt.Printf("Keeping %d hours of consensuses saves:\n", h)
		fmt.Printf("	%d entries [%s] a month: %.2f\n", countAcc, sizeAcc.String(), percentTotal)
		fmt.Printf("	%d entries [%s] per consensus: %.2f\n", countCons, sizeCons.String(), percentTotal)
		fmt.Printf("	%d entries [%s] per diff: %.2f\n", countCons, sizeCons.String(), percentDiff)
	}
}

func main() {
	tarpaths, err := filepath.Glob("consensuses-*.tar.*")
	if err != nil {
		log.Fatal(err)
	}

	cst := ConsensusStats{}
	cst.HoursMissingIds = make([][]string, 100)

	for _, tp := range tarpaths {
		fmt.Printf("Parsing %s\n", tp)
		ext := filepath.Ext(tp)
		var catcmd string
		switch ext {
		case ".xz":
			catcmd = "xzcat"
		case ".bz2":
			catcmd = "bzcat"
		case ".gz":
			catcmd = "zcat"
		default:
			log.Printf("Unsupported format .tar%s in %s", ext, tp)
			continue
		}
		cmd := exec.Command(catcmd, tp)
		fr, err := cmd.StdoutPipe()
		cmd.Stderr = os.Stderr
		if err != nil {
			log.Fatal(err)
		}
		defer fr.Close()
		if err := cmd.Start(); err != nil {
			log.Fatal(err)
		}
		tr := tar.NewReader(fr)
		cst.Data = analyze(cst.Data, tr)
	}
	results(cst)
}
